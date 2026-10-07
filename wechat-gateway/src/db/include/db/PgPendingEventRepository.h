#pragma once

#include <memory>
#include <string>

#include "db/PendingEventRepository.h"

class PgPool;

// 基于 PostgreSQL 的 pending_events 队列 + notify_dispatch 台账（设计文档
// 13.4）。claimDue 在单个语句（立即提交）中使用 FOR UPDATE SKIP LOCKED，
// 因此两个 pod 绝不会处理同一行；幂等性通过
// 对 (event_id, target_user, channel) 的 INSERT ... ON CONFLICT DO NOTHING 实现。
// 仅在定义 HAS_LIBPQ 时编译（见 CMakeLists.txt）。
class PgPendingEventRepository : public PendingEventRepository {
public:
    // `claimed_by` 在 pending_events.claimed_by 中标识当前 pod（HOSTNAME）。
    PgPendingEventRepository(std::shared_ptr<PgPool> pool,
                             std::string claimed_by);

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
    long countByStatus(const std::string& status) const override;

private:
    std::shared_ptr<PgPool> pool_;
    std::string claimed_by_;
};
