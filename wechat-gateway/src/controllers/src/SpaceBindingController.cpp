#include "controllers/SpaceBindingController.h"

#include <nlohmann/json.hpp>

#include <utility>

#include "utils/HttpResponseUtil.h"
#include "utils/RequestUtil.h"

SpaceBindingController::SpaceBindingController(
    std::shared_ptr<SpaceBindingService> service)
    : service_(std::move(service)) {}

void SpaceBindingController::list(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);
    const auto r = service_->list(id.role);
    callback(http_util::jsonResponse(r.status, r.body));
}

void SpaceBindingController::bind(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);

    nlohmann::json body;
    if (!req->getBody().empty()) {
        try {
            body = nlohmann::json::parse(req->getBody());
        } catch (const std::exception&) {
            callback(http_util::error(drogon::k400BadRequest,
                                      "invalid JSON body"));
            return;
        }
    }
    if (!body.is_object() ||
        !body.contains("user_id") || !body["user_id"].is_string() ||
        !body.contains("space_id") || !body["space_id"].is_string()) {
        callback(http_util::error(drogon::k400BadRequest,
                                  "user_id and space_id must be strings"));
        return;
    }

    const auto r = service_->bind(id.role, body["user_id"].get<std::string>(),
                                  body["space_id"].get<std::string>());
    callback(http_util::jsonResponse(r.status, r.body));
}

void SpaceBindingController::unbind(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);
    const std::string userId = req->getParameter("user_id");
    const std::string spaceId = req->getParameter("space_id");
    if (userId.empty() || spaceId.empty()) {
        callback(http_util::error(drogon::k400BadRequest,
                                  "user_id and space_id required"));
        return;
    }

    const auto r = service_->unbind(id.role, userId, spaceId);
    callback(http_util::jsonResponse(r.status, r.body));
}
