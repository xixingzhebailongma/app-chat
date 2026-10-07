#pragma once

#include <memory>
#include <string>

#include "db/SpaceRepository.h"
#include "db/UserRoleRepository.h"
#include "db/UserSpacesRepository.h"
#include "db/WechatBindingRepository.h"
#include "utils/ServiceResult.h"

// 管理员给教师绑定/解绑教室（设计文档 7.7 / 7.10）。全部操作要求 admin 角色。
class SpaceBindingService {
public:
    SpaceBindingService(std::shared_ptr<UserSpacesRepository> userSpaces,
                        std::shared_ptr<UserRoleRepository> userRoles,
                        std::shared_ptr<SpaceRepository> spaces,
                        std::shared_ptr<WechatBindingRepository> bindings);

    // GET /api/miniapp/space-bindings：教师名单 + 各自绑定教室。
    ServiceResult list(const std::string& role);
    // POST /api/miniapp/space-bindings：绑定（幂等）。
    ServiceResult bind(const std::string& role, const std::string& user_id,
                       const std::string& space_id);
    // DELETE /api/miniapp/space-bindings：解绑（幂等）。
    ServiceResult unbind(const std::string& role, const std::string& user_id,
                         const std::string& space_id);

private:
    std::shared_ptr<UserSpacesRepository> userSpaces_;
    std::shared_ptr<UserRoleRepository> userRoles_;
    std::shared_ptr<SpaceRepository> spaces_;
    std::shared_ptr<WechatBindingRepository> bindings_;
};
