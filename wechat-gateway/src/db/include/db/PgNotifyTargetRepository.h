#pragma once

#include <memory>
#include <string>
#include <vector>

#include "db/NotifyTargetRepository.h"

class PgPool;

// 基于 PostgreSQL 的收件人解析仓库（设计文档 9.4 第 2 步）：
// usersByRole 读 user_roles（角色只读模型，权威源 go-backend，
// 经 /internal/user-roles/sync 同步）；usersBySpace 读 user_spaces。
// 仅在定义 HAS_LIBPQ 时编译（见 CMakeLists.txt）。
class PgNotifyTargetRepository : public NotifyTargetRepository {
public:
    explicit PgNotifyTargetRepository(std::shared_ptr<PgPool> pool);

    std::vector<std::string> usersByRole(const std::string& role) const override;
    std::vector<std::string> usersBySpace(
        const std::string& space_id) const override;

private:
    std::shared_ptr<PgPool> pool_;
};
