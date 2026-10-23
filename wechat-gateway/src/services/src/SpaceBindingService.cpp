#include "services/SpaceBindingService.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>
#include <utility>

#include "db/OperationLogWriter.h"

SpaceBindingService::SpaceBindingService(
    std::shared_ptr<UserSpacesRepository> userSpaces,
    std::shared_ptr<UserRoleRepository> userRoles,
    std::shared_ptr<SpaceRepository> spaces,
    std::shared_ptr<WechatBindingRepository> bindings,
    std::shared_ptr<OperationLogRepository> opLogRepo,
    std::string opLogFile)
    : userSpaces_(std::move(userSpaces)),
      userRoles_(std::move(userRoles)),
      spaces_(std::move(spaces)),
      bindings_(std::move(bindings)),
      opLogRepo_(std::move(opLogRepo)),
      opLogFile_(std::move(opLogFile)) {}

ServiceResult SpaceBindingService::list(const std::string& role) {
    if (role != "admin") {
        return ServiceResult::error(drogon::k403Forbidden,
                                    "admin role required");
    }

    nlohmann::json arr = nlohmann::json::array();
    for (const auto& userId : userRoles_->usersByRole("teacher")) {
        std::string name = userId;
        if (auto binding = bindings_->findByUser("miniapp", userId)) {
            if (!binding->name.empty()) {
                name = binding->name;
            }
        }
        nlohmann::json spaceIds = nlohmann::json::array();
        for (const auto& sid : userSpaces_->spacesForUser(userId)) {
            spaceIds.push_back(sid);
        }
        arr.push_back({{"user_id", userId},
                       {"name", name},
                       {"space_ids", spaceIds}});
    }
    return ServiceResult::ok(nlohmann::json{{"teachers", arr}});
}

ServiceResult SpaceBindingService::bind(const std::string& role,
                                        const std::string& operatorUserId,
                                        const std::string& teacherUserId,
                                        const std::string& spaceId) {
    if (role != "admin") {
        return ServiceResult::error(drogon::k403Forbidden,
                                    "admin role required");
    }
    if (!spaces_->findById(spaceId)) {
        return ServiceResult::error(drogon::k404NotFound, "space not found");
    }
    const auto teachers = userRoles_->usersByRole("teacher");
    if (std::find(teachers.begin(), teachers.end(), teacherUserId) ==
        teachers.end()) {
        return ServiceResult::error(drogon::k400BadRequest,
                                    "user is not a teacher");
    }
    userSpaces_->add(teacherUserId, spaceId);

    OperationLogEntry entry;
    entry.op_type = "space_bind";
    entry.user_id = operatorUserId;
    entry.space_id = spaceId;
    entry.scene_id = "";
    entry.success_count = 1;
    entry.failed_count = 0;
    entry.detail = nlohmann::json{{"teacher_user_id", teacherUserId}}.dump();
    oplog::writeOperationLog(opLogRepo_.get(), entry, opLogFile_);

    return ServiceResult::ok(nlohmann::json{{"ok", true}});
}

ServiceResult SpaceBindingService::unbind(const std::string& role,
                                          const std::string& operatorUserId,
                                          const std::string& teacherUserId,
                                          const std::string& spaceId) {
    if (role != "admin") {
        return ServiceResult::error(drogon::k403Forbidden,
                                    "admin role required");
    }
    userSpaces_->remove(teacherUserId, spaceId);

    OperationLogEntry entry;
    entry.op_type = "space_unbind";
    entry.user_id = operatorUserId;
    entry.space_id = spaceId;
    entry.scene_id = "";
    entry.success_count = 1;
    entry.failed_count = 0;
    entry.detail = nlohmann::json{{"teacher_user_id", teacherUserId}}.dump();
    oplog::writeOperationLog(opLogRepo_.get(), entry, opLogFile_);

    return ServiceResult::ok(nlohmann::json{{"ok", true}});
}
