#include "controllers/UserRoleController.h"

#include <nlohmann/json.hpp>

#include <string>
#include <utility>
#include <vector>

#include "utils/HttpResponseUtil.h"

UserRoleController::UserRoleController(std::shared_ptr<UserRoleRepository> repo)
    : repo_(std::move(repo)) {}

void UserRoleController::sync(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    nlohmann::json body;
    try {
        body = nlohmann::json::parse(req->getBody());
    } catch (const std::exception&) {
        callback(http_util::error(drogon::k400BadRequest, "invalid JSON body"));
        return;
    }

    if (!body.is_object() || !body.contains("user_id") ||
        !body["user_id"].is_string() || body["user_id"].get<std::string>().empty()) {
        callback(http_util::error(drogon::k400BadRequest,
                                  "user_id is required and must be a string"));
        return;
    }

    std::vector<std::string> roles;
    if (body.contains("roles")) {
        if (!body["roles"].is_array()) {
            callback(http_util::error(drogon::k400BadRequest,
                                      "roles must be an array of strings"));
            return;
        }
        for (const auto& r : body["roles"]) {
            if (!r.is_string()) {
                callback(http_util::error(drogon::k400BadRequest,
                                          "roles must be an array of strings"));
                return;
            }
            roles.push_back(r.get<std::string>());
        }
    }

    repo_->setRoles(body["user_id"].get<std::string>(), roles);
    callback(http_util::ok(nlohmann::json{{"ok", true}}));
}
