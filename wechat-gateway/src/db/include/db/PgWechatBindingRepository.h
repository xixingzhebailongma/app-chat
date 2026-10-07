#pragma once

#include <memory>
#include <optional>
#include <string>

#include "db/WechatBindingRepository.h"

class PgPool;

// 基于 PostgreSQL 的 wechat_bindings 仓库（sql/migration_v1.sql）。
// 仅在定义 HAS_LIBPQ 时编译（见 CMakeLists.txt）。
class PgWechatBindingRepository : public WechatBindingRepository {
public:
    explicit PgWechatBindingRepository(std::shared_ptr<PgPool> pool);

    std::optional<WechatBinding> findByOpenid(
        const std::string& channel, const std::string& openid) const override;
    std::optional<WechatBinding> findByUser(
        const std::string& channel, const std::string& user_id) const override;
    void save(const WechatBinding& binding) override;

private:
    std::shared_ptr<PgPool> pool_;
};
