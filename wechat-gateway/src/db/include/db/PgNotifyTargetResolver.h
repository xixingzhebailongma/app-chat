#pragma once

#include <memory>
#include <string>
#include <vector>

#include "db/NotifyTargetResolver.h"

class PgPool;

// user_notify_bindings 的 PostgreSQL 实现（读 + 写）。仅在 HAS_LIBPQ 下编译。
// 单实例既作 INotifyTargetResolver（读）又作 NotifyBindingRepository（写）。
class PgNotifyTargetResolver : public INotifyTargetResolver,
                               public NotifyBindingRepository {
public:
    explicit PgNotifyTargetResolver(std::shared_ptr<PgPool> pool);

    TargetResolution resolve(const std::string& channel,
                             const std::string& user_id) override;
    bool save(const NotifyBinding& b) override;
    bool remove(const std::string& channel,
                const std::string& user_id) override;
    std::vector<NotifyBinding> list(const std::string& channel) override;

private:
    std::shared_ptr<PgPool> pool_;
};
