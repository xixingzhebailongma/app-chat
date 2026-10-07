#pragma once

#include <memory>
#include <string>

#include "db/NotifyTargetResolver.h"
#include "utils/ServiceResult.h"

// 管理员导入「内部 user_id -> 渠道外部 ID」映射（钉钉/企微 userid 等，P1 管理员
// 导入；OAuth 自助绑定留 P2）。全部操作要求 admin 角色。
class NotifyBindingService {
public:
    explicit NotifyBindingService(std::shared_ptr<NotifyBindingRepository> repo);

    // GET /api/notify/bindings?channel=dingtalk：该渠道的绑定列表。
    ServiceResult list(const std::string& role, const std::string& channel);
    // POST /api/notify/bindings：upsert 一条绑定。
    ServiceResult save(const std::string& role, const std::string& channel,
                       const std::string& user_id,
                       const std::string& external_id,
                       const std::string& external_extra);
    // DELETE /api/notify/bindings?channel=&user_id=：删除一条绑定。
    ServiceResult remove(const std::string& role, const std::string& channel,
                         const std::string& user_id);

private:
    std::shared_ptr<NotifyBindingRepository> repo_;
};
