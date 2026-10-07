#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

// 等待后台重试的暂存事件（设计文档 13.4）。
struct PendingEvent {
    long id = 0;
    nlohmann::json payload;
    int retry_count = 0;  // 已尝试的重试次数
    std::string last_error;
};

// 预留 (event_id, target_user, channel) 分发槽位的结果。
enum class DispatchClaim {
    Claimed,      // 调用方已插入该行，可以立即发送
    AlreadyDone,  // 已存在对应行 -> 跳过（已送达）
};

// 降级模式队列 + 幂等台账（设计文档 13.4）。生产
// 后端为 PostgreSQL（PgPendingEventRepository）；内存实现用于
// 开发与测试。
class PendingEventRepository {
public:
    virtual ~PendingEventRepository() = default;

    virtual void enqueue(const nlohmann::json& payload) = 0;

    // 领取最多 `limit` 个到期事件。Postgres 实现使用
    // SELECT ... FOR UPDATE SKIP LOCKED 的单个语句（立即
    // 提交）；内存实现则通过互斥锁串行化。
    virtual std::vector<PendingEvent> claimDue(int limit) = 0;

    virtual void markSuccess(long id) = 0;
    // 递增 retry_count，并在 `delay_ms` 后安排下一次尝试。
    virtual void markRetry(long id, int64_t delay_ms,
                           const std::string& error) = 0;
    virtual void markDeadLetter(long id, const std::string& error) = 0;

    // 幂等性：对 (event_id, target_user, channel) 执行
    // INSERT ... ON CONFLICT DO NOTHING。当本调用方插入了该行时返回 Claimed。
    virtual DispatchClaim tryMarkDispatched(const std::string& event_id,
                                            const std::string& target_user,
                                            const std::string& channel,
                                            const std::string& notify_id) = 0;
    // 发送失败时回滚一次预留，释放槽位。
    virtual void clearDispatched(const std::string& event_id,
                                 const std::string& target_user,
                                 const std::string& channel) = 0;

    // `status` 状态下的排队行数（pending | processing | dead_letter）。
    // 供管理平台的死信视图和测试使用。
    virtual long countByStatus(const std::string& status) const = 0;
};

// 面向开发与测试的内存实现。claimDue 在互斥锁下将行标记为 "processing"
// （相当于 FOR UPDATE SKIP LOCKED）；markSuccess 将其移除。这里没有对遗留
// "processing" 行的崩溃回收——真正的 pod 崩溃
// 回收仅属于 Postgres 的职责（见 schema 中的 claimed_at 列）。
class InMemoryPendingEventRepository : public PendingEventRepository {
public:
    void enqueue(const nlohmann::json& payload) override;

    std::vector<PendingEvent> claimDue(int limit) override;

    void markSuccess(long id) override;
    void markRetry(long id, int64_t delay_ms, const std::string& error) override;
    void markDeadLetter(long id, const std::string& error) override;

    DispatchClaim tryMarkDispatched(const std::string& event_id,
                                    const std::string& target_user,
                                    const std::string& channel,
                                    const std::string& notify_id) override;
    void clearDispatched(const std::string& event_id,
                         const std::string& target_user,
                         const std::string& channel) override;

    // --- 测试辅助 ---
    struct Row {
        long id;
        nlohmann::json payload;
        int retry_count;
        std::string status;
        std::string last_error;
    };
    std::vector<Row> snapshot() const;
    size_t deadLetterCount() const;
    size_t dispatchedCount() const;
    long countByStatus(const std::string& status) const override;

private:
    struct Entry {
        long id;
        nlohmann::json payload;
        int retry_count = 0;
        std::string status = "pending";  // pending | processing | dead_letter
        std::string last_error;
        std::chrono::steady_clock::time_point next_retry_at;
    };

    Entry* findByStatus(long id, const char* status);

    mutable std::mutex mutex_;
    std::vector<Entry> rows_;
    std::unordered_set<std::string> dispatched_;
    long next_id_ = 1;
};
