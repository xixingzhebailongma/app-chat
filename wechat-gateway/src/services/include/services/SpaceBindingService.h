#pragma once

#include <memory>
#include <string>

#include "db/OperationLogRepository.h"
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
                        std::shared_ptr<WechatBindingRepository> bindings,
                        std::shared_ptr<OperationLogRepository> opLogRepo,
                        std::string opLogFile);

    // GET /api/miniapp/space-bindings：教师名单 + 各自绑定教室。
    ServiceResult list(const std::string& role);
    // POST /api/miniapp/space-bindings：绑定（幂等，成功写运维日志）。
    ServiceResult bind(const std::string& role,
                       const std::string& operatorUserId,
                       const std::string& teacherUserId,
                       const std::string& spaceId);
    // DELETE /api/miniapp/space-bindings：解绑（幂等，成功写运维日志）。
    ServiceResult unbind(const std::string& role,
                         const std::string& operatorUserId,
                         const std::string& teacherUserId,
                         const std::string& spaceId);

private:
    std::shared_ptr<UserSpacesRepository> userSpaces_;
    std::shared_ptr<UserRoleRepository> userRoles_;
    std::shared_ptr<SpaceRepository> spaces_;
    std::shared_ptr<WechatBindingRepository> bindings_;
    std::shared_ptr<OperationLogRepository> opLogRepo_;  // 可空（内存模式）
    std::string opLogFile_;  // 文件兜底路径，空串=禁用
};
