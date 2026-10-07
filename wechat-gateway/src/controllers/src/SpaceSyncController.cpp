#include "controllers/SpaceSyncController.h"

#include <nlohmann/json.hpp>

#include <string>
#include <utility>
#include <vector>

#include "utils/HttpResponseUtil.h"

SpaceSyncController::SpaceSyncController(
    std::shared_ptr<SpaceSyncService> service)
    : service_(std::move(service)) {}

void SpaceSyncController::sync(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    nlohmann::json body;
    try {
        body = nlohmann::json::parse(req->getBody());
    } catch (const std::exception&) {
        callback(http_util::error(drogon::k400BadRequest, "invalid JSON body"));
        return;
    }

    if (!body.is_object() || !body.contains("spaces") ||
        !body["spaces"].is_array()) {
        callback(http_util::error(drogon::k400BadRequest,
                                  "spaces must be an array"));
        return;
    }

    std::vector<Space> spaces;
    for (const auto& s : body["spaces"]) {
        if (!s.is_object()) {
            callback(http_util::error(drogon::k400BadRequest,
                                      "each space must be an object"));
            return;
        }
        Space sp;
        sp.space_id = s.value("space_id", "");
        sp.name = s.value("name", "");
        sp.type = s.value("type", "standard");
        if (sp.space_id.empty()) {
            callback(http_util::error(drogon::k400BadRequest,
                                      "space_id is required"));
            return;
        }
        spaces.push_back(std::move(sp));
    }

    const auto r = service_->sync(spaces);
    callback(http_util::jsonResponse(r.status, r.body));
}
