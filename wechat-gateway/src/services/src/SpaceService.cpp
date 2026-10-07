#include "services/SpaceService.h"

#include <nlohmann/json.hpp>

#include <utility>

SpaceService::SpaceService(std::shared_ptr<SpaceRepository> spaces,
                           std::shared_ptr<UserSpacesRepository> userSpaces)
    : spaces_(std::move(spaces)), userSpaces_(std::move(userSpaces)) {}

ServiceResult SpaceService::list(const std::string& userId,
                                 const std::string& role) {
    nlohmann::json arr = nlohmann::json::array();

    if (role == "admin") {
        for (const auto& space : spaces_->listAll()) {
            arr.push_back({{"space_id", space.space_id},
                           {"name", space.name},
                           {"type", space.type}});
        }
        return ServiceResult::ok(nlohmann::json{{"spaces", arr}});
    }

    if (role == "teacher") {
        for (const auto& spaceId : userSpaces_->spacesForUser(userId)) {
            if (auto space = spaces_->findById(spaceId)) {
                arr.push_back({{"space_id", space->space_id},
                               {"name", space->name},
                               {"type", space->type}});
            }
        }
        return ServiceResult::ok(nlohmann::json{{"spaces", arr}});
    }

    return ServiceResult::error(drogon::k403Forbidden, "forbidden");
}
