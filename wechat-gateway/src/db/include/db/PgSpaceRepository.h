#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "db/SpaceRepository.h"

class PgPool;

// 基于 PostgreSQL 的 spaces 仓库（sql/migration_v1.sql + v16）。
// 仅在定义 HAS_LIBPQ 时编译（见 CMakeLists.txt）。
class PgSpaceRepository : public SpaceRepository {
public:
    explicit PgSpaceRepository(std::shared_ptr<PgPool> pool);

    std::vector<Space> listAll() const override;
    std::vector<Space> listEnabled() const override;
    std::optional<Space> findById(const std::string& space_id) const override;
    void upsert(const Space& space) override;
    void updateMeta(const std::string& space_id, const std::string& name,
                    const std::string& type) override;
    bool disable(const std::string& space_id) override;
    void remove(const std::string& space_id) override;

private:
    std::shared_ptr<PgPool> pool_;
};
