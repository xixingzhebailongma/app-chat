#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>

#include "db/PendingEventRepository.h"

// 重新分派一个挂起事件的结果。
struct RetryOutcome {
    bool delivered = false;   // 每个 (user,channel) 均已送达
    std::string error;        // 人类可读的失败原因
    std::string failed_detail;  // 例如 "u1:wechat_miniapp,u1:sms"
};

// 重试调度参数（设计文档 13.4 / 13.9）。时长以毫秒保存，
// 以便测试可以缩短它们；fromJson() 从配置读取秒数。
struct RetryConfig {
    int max_attempts = 5;                                // 进入死信前的重试次数
    std::chrono::milliseconds base_delay{60 * 1000};     // 每次重试翻倍（×2）
    std::chrono::milliseconds max_delay{30 * 60 * 1000}; // 每次重试的上限
    double jitter_ratio = 0.2;                           // ±20%
    std::chrono::milliseconds poll_interval{1000};
    int batch_size = 50;

    // 读取 notify.retry.*（秒）。缺失的键回退到默认值。
    static RetryConfig fromJson(const nlohmann::json& notify);
};

// 后台重试工作线程：持有独立线程，通过 FOR UPDATE SKIP LOCKED（在 Postgres
// 仓库中）领取到期事件，经由注入的分派器重新分派，并以指数退避调度重试，
// 直到事件送达或进入死信。stop() 通知并 join 线程以实现优雅退出。
class RetryWorker {
public:
    using Dispatcher =
        std::function<RetryOutcome(const nlohmann::json& payload)>;

    RetryWorker(std::shared_ptr<PendingEventRepository> repo,
                Dispatcher dispatch, RetryConfig cfg);

    RetryWorker(const RetryWorker&) = delete;
    RetryWorker& operator=(const RetryWorker&) = delete;

    // 启动工作线程。幂等。
    void start();
    // 通知线程并 join。未运行时调用也安全。
    void stop();

    std::chrono::milliseconds backoff(int retry_count) const;

private:
    void run();

    std::shared_ptr<PendingEventRepository> repo_;
    Dispatcher dispatch_;
    RetryConfig cfg_;

    std::atomic<bool> running_{false};
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    mutable std::mt19937 rng_;  // 每个工作线程独立播种，使各 pod 的抖动不同
};
