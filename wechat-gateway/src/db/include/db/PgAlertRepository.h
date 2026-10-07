#pragma once

#include <memory>
#include <string>
#include <vector>

#include "db/AlertRepository.h"

class PgPool;

// 基于 PostgreSQL 的 alert_handles 仓库（sql/migration_v1.sql）。
// 仅在定义 HAS_LIBPQ 时编译（见 CMakeLists.txt）。
class PgAlertRepository : public AlertRepository {
public:
    explicit PgAlertRepository(std::shared_ptr<PgPool> pool);

    std::vector<Alert> query(const std::vector<std::string>& space_ids,
                             const AlertFilter& f, int limit,
                             int offset) const override;
    int count(const std::vector<std::string>& space_ids,
              const AlertFilter& f) const override;
    bool upsert(const Alert& alert) override;
    bool handle(const std::string& alert_id,
                const std::string& operator_id,
                const std::string& remark,
                const std::string& status) override;
    std::optional<Alert> findById(const std::string& alert_id) const override;
    AlertStats stats(const std::vector<std::string>& space_ids) const override;

private:
    std::shared_ptr<PgPool> pool_;
};
