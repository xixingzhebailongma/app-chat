#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "db/AccessRecordRepository.h"

class PgPool;

// 基于 PostgreSQL 的 access_records 仓库（sql/migration_v6.sql）。
// 仅在定义 HAS_LIBPQ 时编译（见 CMakeLists.txt）。
class PgAccessRecordRepository : public AccessRecordRepository {
public:
    explicit PgAccessRecordRepository(std::shared_ptr<PgPool> pool);

    void insert(const AccessRecord& r) override;
    std::vector<AccessRecord> query(
        const std::vector<std::string>& space_ids,
        const std::optional<std::string>& from,
        const std::optional<std::string>& to, int limit, int offset,
        const std::optional<std::string>& authType = std::nullopt) const override;
    int count(const std::vector<std::string>& space_ids,
              const std::optional<std::string>& from,
              const std::optional<std::string>& to,
              const std::optional<std::string>& authType = std::nullopt) const override;

private:
    std::shared_ptr<PgPool> pool_;
};
