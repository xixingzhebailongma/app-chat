#pragma once

#include <memory>
#include <string>

#include "db/SubscriptionRepository.h"

class PgPool;

// 基于 PostgreSQL 的 subscriptions 仓库（sql/migration_v5.sql，模板级额度）。
// 仅在定义 HAS_LIBPQ 时编译（见 CMakeLists.txt）。
class PgSubscriptionRepository : public SubscriptionRepository {
public:
    explicit PgSubscriptionRepository(std::shared_ptr<PgPool> pool);

    void grant(const std::string& user_id, const std::string& template_id,
               bool long_term) override;
    bool reserve(const std::string& user_id,
                 const std::string& template_id) override;
    void refill(const std::string& user_id,
                const std::string& template_id) override;
    void revoke(const std::string& user_id,
                const std::string& template_id) override;
    void revokeAll(const std::string& user_id) override;
    int quotaOf(const std::string& user_id,
                const std::string& template_id) const override;

private:
    std::shared_ptr<PgPool> pool_;
};
