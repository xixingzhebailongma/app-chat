#pragma once

#include <memory>
#include <string>

#include "db/OaNotifyLogRepository.h"

class PgPool;

// 基于 PostgreSQL 的 oa_notify_logs 仓库（sql/migration_v10.sql）。
// 仅在定义 HAS_LIBPQ 时编译（见 CMakeLists.txt）。
class PgOaNotifyLogRepository : public OaNotifyLogRepository {
public:
    explicit PgOaNotifyLogRepository(std::shared_ptr<PgPool> pool);

    void save(const OaNotifyLogEntry& entry) override;

    EventIdCheck existsByEventId(const std::string& event_id) const override;

    DedupCheck hasSuccessToday(const std::string& student_no,
                               const std::string& space_id) const override;

private:
    std::shared_ptr<PgPool> pool_;
};
