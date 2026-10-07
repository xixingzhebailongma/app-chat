#include "db/PendingEventRepository.h"

#include <chrono>
#include <utility>

void InMemoryPendingEventRepository::enqueue(const nlohmann::json& payload) {
    std::lock_guard<std::mutex> lock(mutex_);
    Entry e;
    e.id = next_id_++;
    e.payload = payload;
    e.next_retry_at = std::chrono::steady_clock::now();
    rows_.push_back(std::move(e));
}

std::vector<PendingEvent> InMemoryPendingEventRepository::claimDue(int limit) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<PendingEvent> out;
    const auto now = std::chrono::steady_clock::now();
    for (auto& r : rows_) {
        if (static_cast<int>(out.size()) >= limit) {
            break;
        }
        if (r.status != "pending" || r.next_retry_at > now) {
            continue;
        }
        r.status = "processing";
        out.push_back({r.id, r.payload, r.retry_count, r.last_error});
    }
    return out;
}

InMemoryPendingEventRepository::Entry*
InMemoryPendingEventRepository::findByStatus(long id, const char* status) {
    for (auto& r : rows_) {
        if (r.id == id && r.status == status) {
            return &r;
        }
    }
    return nullptr;
}

void InMemoryPendingEventRepository::markSuccess(long id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = rows_.begin(); it != rows_.end(); ++it) {
        if (it->id == id && it->status == "processing") {
            rows_.erase(it);
            return;
        }
    }
}

void InMemoryPendingEventRepository::markRetry(long id, int64_t delay_ms,
                                               const std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    Entry* e = findByStatus(id, "processing");
    if (!e) {
        return;
    }
    e->status = "pending";
    e->retry_count += 1;
    e->last_error = error;
    e->next_retry_at =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);
}

void InMemoryPendingEventRepository::markDeadLetter(long id,
                                                    const std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    Entry* e = findByStatus(id, "processing");
    if (!e) {
        return;
    }
    e->status = "dead_letter";
    e->last_error = error;
}

DispatchClaim InMemoryPendingEventRepository::tryMarkDispatched(
    const std::string& event_id, const std::string& target_user,
    const std::string& channel, const std::string& notify_id) {
    (void)notify_id;
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string key = event_id + "|" + target_user + "|" + channel;
    if (dispatched_.count(key)) {
        return DispatchClaim::AlreadyDone;
    }
    dispatched_.insert(key);
    return DispatchClaim::Claimed;
}

void InMemoryPendingEventRepository::clearDispatched(
    const std::string& event_id, const std::string& target_user,
    const std::string& channel) {
    std::lock_guard<std::mutex> lock(mutex_);
    dispatched_.erase(event_id + "|" + target_user + "|" + channel);
}

std::vector<InMemoryPendingEventRepository::Row>
InMemoryPendingEventRepository::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Row> out;
    out.reserve(rows_.size());
    for (const auto& r : rows_) {
        out.push_back({r.id, r.payload, r.retry_count, r.status, r.last_error});
    }
    return out;
}

size_t InMemoryPendingEventRepository::deadLetterCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t n = 0;
    for (const auto& r : rows_) {
        if (r.status == "dead_letter") {
            ++n;
        }
    }
    return n;
}

size_t InMemoryPendingEventRepository::dispatchedCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return dispatched_.size();
}

long InMemoryPendingEventRepository::countByStatus(
    const std::string& status) const {
    std::lock_guard<std::mutex> lock(mutex_);
    long n = 0;
    for (const auto& r : rows_) {
        if (r.status == status) {
            ++n;
        }
    }
    return n;
}
