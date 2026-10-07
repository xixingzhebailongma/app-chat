#pragma once

#include <memory>
#include <string>
#include <vector>

#include "db/UserRoleRepository.h"

class PgPool;

// 基于 PostgreSQL 的 user_roles 写仓库。setRoles 在单个事务里
// DELETE + 逐条 INSERT，保证全量替换的原子性。
// 仅在定义 HAS_LIBPQ 时编译（见 CMakeLists.txt）。
class PgUserRoleRepository : public UserRoleRepository {
public:
    explicit PgUserRoleRepository(std::shared_ptr<PgPool> pool);

    void setRoles(const std::string& user_id,
                  const std::vector<std::string>& roles) override;
    void removeUser(const std::string& user_id) override;
    std::vector<std::string> usersByRole(const std::string& role) const override;
    std::string roleOf(const std::string& user_id) const override;

private:
    std::shared_ptr<PgPool> pool_;
};
