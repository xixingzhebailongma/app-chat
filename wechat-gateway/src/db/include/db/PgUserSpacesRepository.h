#pragma once

#include <memory>
#include <string>
#include <vector>

#include "db/UserSpacesRepository.h"

class PgPool;

// 基于 PostgreSQL 的 user_spaces 仓库（sql/migration_v1.sql）。
// 仅在定义 HAS_LIBPQ 时编译（见 CMakeLists.txt）。
class PgUserSpacesRepository : public UserSpacesRepository {
public:
    explicit PgUserSpacesRepository(std::shared_ptr<PgPool> pool);

    bool contains(const std::string& user_id,
                  const std::string& space_id) const override;
    std::vector<std::string> spacesForUser(
        const std::string& user_id) const override;
    void add(const std::string& user_id,
             const std::string& space_id) override;
    void remove(const std::string& user_id,
                const std::string& space_id) override;
    void removeBySpace(const std::string& space_id) override;

private:
    std::shared_ptr<PgPool> pool_;
};
