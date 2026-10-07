#pragma once

#include <memory>

#include "db/OperationLogRepository.h"

class PgPool;

// 基于 PostgreSQL 的 operation_logs 仓库（sql/migration_v8.sql）。
// 仅在定义 HAS_LIBPQ 时编译（见 CMakeLists.txt）。
class PgOperationLogRepository : public OperationLogRepository {
public:
    explicit PgOperationLogRepository(std::shared_ptr<PgPool> pool);
    bool insert(const OperationLogEntry& e) override;
    std::vector<OperationLogRow> query(const OperationLogFilter& f, int limit,
                                       int offset) const override;
    int count(const OperationLogFilter& f) const override;

private:
    std::shared_ptr<PgPool> pool_;
};
