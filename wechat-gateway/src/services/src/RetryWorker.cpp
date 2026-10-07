#include "services/RetryWorker.h"

#include <trantor/utils/Logger.h>

#include <random>
#include <utility>

RetryConfig RetryConfig::fromJson(const nlohmann::json& notify) {
    RetryConfig cfg;
    if (!notify.is_object() || !notify.contains("retry")) {
        return cfg;
    }
    const auto& r = notify["retry"];
    cfg.max_attempts = r.value("max_attempts", cfg.max_attempts);
    cfg.base_delay = std::chrono::milliseconds(
        static_cast<long long>(r.value("base_delay_seconds", 60) * 1000));
    cfg.max_delay = std::chrono::milliseconds(
        static_cast<long long>(r.value("max_delay_seconds", 1800) * 1000));
    cfg.jitter_ratio = r.value("jitter_ratio", 0.2);
    cfg.poll_interval = std::chrono::milliseconds(
        static_cast<long long>(r.value("poll_interval_seconds", 1) * 1000));
    cfg.batch_size = r.value("batch_size", 50);
    return cfg;
}

RetryWorker::RetryWorker(std::shared_ptr<PendingEventRepository> repo,
                         Dispatcher dispatch, RetryConfig cfg)
    : repo_(std::move(repo)),
      dispatch_(std::move(dispatch)),
      cfg_(std::move(cfg)),
      rng_(std::random_device{}()) {}

void RetryWorker::start() {
    if (running_.exchange(true)) {
        return;  // 已在运行
    }
    thread_ = std::thread(&RetryWorker::run, this);
}

void RetryWorker::stop() {
    if (!running_.exchange(false)) {
        return;  // 未在运行
    }
    cv_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
}

void RetryWorker::run() {
    while (running_.load()) {
        // 可中断休眠：stop() 通知条件变量以立即唤醒我们。
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait_for(lock, cfg_.poll_interval,
                         [this] { return !running_.load(); });
        }
        if (!running_.load()) {
            break;
        }

        const auto due = repo_->claimDue(cfg_.batch_size);
        for (const auto& e : due) {
            if (!running_.load()) {
                break;
            }
            const RetryOutcome outcome = dispatch_(e.payload);
            const std::string event_id = e.payload.value("event_id", "");
            const int attempt = e.retry_count + 1;

            if (outcome.delivered) {
                repo_->markSuccess(e.id);
                LOG_INFO << "[retry] event_id=" << event_id
                         << " attempt=" << attempt << " result=delivered";
            } else if (e.retry_count + 1 >= cfg_.max_attempts) {
                repo_->markDeadLetter(e.id, outcome.error);
                LOG_WARN << "[retry] event_id=" << event_id
                         << " attempt=" << attempt
                         << " result=dead_letter failed=" << outcome.failed_detail
                         << " error=" << outcome.error;
            } else {
                repo_->markRetry(e.id, backoff(e.retry_count).count(),
                                 outcome.error);
                LOG_WARN << "[retry] event_id=" << event_id
                         << " attempt=" << attempt
                         << " result=failed failed=" << outcome.failed_detail
                         << " error=" << outcome.error;
            }
        }
    }
}

std::chrono::milliseconds RetryWorker::backoff(int retry_count) const {
    long long ms = cfg_.base_delay.count();
    const long long cap = cfg_.max_delay.count();
    for (int i = 0; i < retry_count && ms < cap; ++i) {
        ms = ms * 2 > cap ? cap : ms * 2;
    }
    if (cfg_.jitter_ratio > 0.0 && ms > 0) {
        std::uniform_real_distribution<double> dist(-cfg_.jitter_ratio,
                                                    cfg_.jitter_ratio);
        ms = static_cast<long long>(static_cast<double>(ms) * (1.0 + dist(rng_)));
        if (ms < 1) {
            ms = 1;
        }
    }
    return std::chrono::milliseconds(ms);
}
