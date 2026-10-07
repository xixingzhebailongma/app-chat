#include "services/SpaceBindingService.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>
#include <utility>

SpaceBindingService::SpaceBindingService(
    std::shared_ptr<UserSpacesRepository> userSpaces,
    std::shared_ptr<UserRoleRepository> userRoles,
    std::shared_ptr<SpaceRepository> spaces,
    std::shared_ptr<WechatBindingRepository> bindings)
    : userSpaces_(std::move(userSpaces)),
      userRoles_(std::move(userRoles)),
      spaces_(std::move(spaces)),
      bindings_(std::move(bindings)) {}

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
                                        const std::string& user_id,
                                        const std::string& space_id) {
    if (role != "admin") {
        return ServiceResult::error(drogon::k403Forbidden,
                                    "admin role required");
    }
    if (!spaces_->findById(space_id)) {
        return ServiceResult::error(drogon::k404NotFound, "space not found");
    }
    const auto teachers = userRoles_->usersByRole("teacher");
    if (std::find(teachers.begin(), teachers.end(), user_id) ==
        teachers.end()) {
        return ServiceResult::error(drogon::k400BadRequest,
                                    "user is not a teacher");
    }
    userSpaces_->add(user_id, space_id);
    return ServiceResult::ok(nlohmann::json{{"ok", true}});
}

ServiceResult SpaceBindingService::unbind(const std::string& role,
                                          const std::string& user_id,
                                          const std::string& space_id) {
    if (role != "admin") {
        return ServiceResult::error(drogon::k403Forbidden,
                                    "admin role required");
    }
    userSpaces_->remove(user_id, space_id);
    return ServiceResult::ok(nlohmann::json{{"ok", true}});
}
