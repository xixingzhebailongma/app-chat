#include "services/SpaceService.h"

#include <nlohmann/json.hpp>

#include <string>
#include <utility>

#include "utils/HttpResponseUtil.h"

SpaceService::SpaceService(std::shared_ptr<SpaceRepository> spaces,
                           std::shared_ptr<UserSpacesRepository> userSpaces,
                           std::shared_ptr<SpaceTypeRepository> spaceTypes)
    : spaces_(std::move(spaces)),
      userSpaces_(std::move(userSpaces)),
      spaceTypes_(std::move(spaceTypes)) {}

ServiceResult SpaceService::list(const std::string& userId,
                                 const std::string& role) {
    nlohmann::json arr = nlohmann::json::array();

    if (role == "admin") {
        for (const auto& space : spaces_->listEnabled()) {
            arr.push_back({{"space_id", space.space_id},
                           {"name", space.name},
                           {"type", space.type}});
        }
        return ServiceResult::ok(nlohmann::json{{"spaces", arr}});
    }

    if (role == "teacher") {
        for (const auto& spaceId : userSpaces_->spacesForUser(userId)) {
            if (auto space = spaces_->findById(spaceId)) {
                if (!space->enabled) {
                    continue;  // 停用空间对教师隐藏
                }
                arr.push_back({{"space_id", space->space_id},
                               {"name", space->name},
                               {"type", space->type}});
            }
        }
        return ServiceResult::ok(nlohmann::json{{"spaces", arr}});
    }

    return ServiceResult::error(drogon::k403Forbidden, "forbidden");
}

ServiceResult SpaceService::adminList(const std::string& role) {
    if (role != "admin") {
        return ServiceResult::error(drogon::k403Forbidden,
                                    "admin role required");
    }
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& space : spaces_->listAll()) {
        arr.push_back({{"space_id", space.space_id},
                       {"name", space.name},
                       {"type", space.type},
                       {"enabled", space.enabled},
                       {"source", space.source}});
    }
    return ServiceResult::ok(nlohmann::json{{"spaces", arr}});
}

ServiceResult SpaceService::create(const std::string& role,
                                   const std::string& name,
                                   const std::string& type,
                                   const std::string& spaceId) {
    if (role != "admin") {
        return ServiceResult::error(drogon::k403Forbidden,
                                    "admin role required");
    }
    if (name.empty() || type.empty()) {
        return ServiceResult::error(drogon::k400BadRequest,
                                    "name and type are required");
    }
    if (!spaceTypes_->findByCode(type)) {
        return ServiceResult::error(drogon::k400BadRequest,
                                    "type not found in space_types");
    }

    const std::string id =
        spaceId.empty() ? http_util::newId("spc") : spaceId;
    if (spaces_->findById(id)) {
        return ServiceResult::error(drogon::k409Conflict,
                                    "space already exists");
    }

    Space s;
    s.space_id = id;
    s.name = name;
    s.type = type;
    s.enabled = true;
    s.source = "admin";
    spaces_->upsert(s);
    return ServiceResult::ok(nlohmann::json{{"ok", true}, {"space_id", id}});
}

ServiceResult SpaceService::update(const std::string& role,
                                   const std::string& spaceId,
                                   std::optional<std::string> name,
                                   std::optional<std::string> type,
                                   std::optional<bool> enabled) {
    if (role != "admin") {
        return ServiceResult::error(drogon::k403Forbidden,
                                    "admin role required");
    }
    const auto existing = spaces_->findById(spaceId);
    if (!existing) {
        return ServiceResult::error(drogon::k404NotFound, "space not found");
    }

    std::string newName = existing->name;
    std::string newType = existing->type;
    bool metaChanged = false;
    if (name.has_value()) {
        if (name->empty()) {
            return ServiceResult::error(drogon::k400BadRequest,
                                        "name must not be empty");
        }
        newName = *name;
        metaChanged = true;
    }
    if (type.has_value()) {
        if (!spaceTypes_->findByCode(*type)) {
            return ServiceResult::error(drogon::k400BadRequest,
                                        "type not found in space_types");
        }
        newType = *type;
        metaChanged = true;
    }
    if (metaChanged) {
        spaces_->updateMeta(spaceId, newName, newType);
    }
    if (enabled.has_value()) {
        spaces_->setEnabled(spaceId, *enabled);
    }
    return ServiceResult::ok(nlohmann::json{{"ok", true}});
}

ServiceResult SpaceService::disable(const std::string& role,
                                    const std::string& spaceId) {
    if (role != "admin") {
        return ServiceResult::error(drogon::k403Forbidden,
                                    "admin role required");
    }
    if (!spaces_->findById(spaceId)) {
        return ServiceResult::error(drogon::k404NotFound, "space not found");
    }
    spaces_->setEnabled(spaceId, false);
    return ServiceResult::ok(nlohmann::json{{"ok", true}});
}
