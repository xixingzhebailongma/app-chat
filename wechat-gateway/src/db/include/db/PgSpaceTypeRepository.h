#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "db/SpaceTypeRepository.h"

class PgPool;

// 基于 PostgreSQL 的 space_types 仓库（sql/migration_v15.sql）。
// 仅在定义 HAS_LIBPQ 时编译（见 CMakeLists.txt）。
class PgSpaceTypeRepository : public SpaceTypeRepository {
public:
    explicit PgSpaceTypeRepository(std::shared_ptr<PgPool> pool);

    std::vector<SpaceType> listAll() const override;
    std::optional<SpaceType> findByCode(const std::string& code) const override;
    void upsert(const SpaceType& t) override;
    bool disable(const std::string& code) override;
    void setSortOrder(const std::vector<std::string>& codes) override;
    int maxSortOrder() const override;

private:
    std::shared_ptr<PgPool> pool_;
};
