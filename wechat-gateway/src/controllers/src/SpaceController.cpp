#include "controllers/SpaceController.h"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <utility>

#include "utils/HttpResponseUtil.h"
#include "utils/RequestUtil.h"

namespace {

// 解析必填的 JSON object body；失败回 400 并返回 false。
bool parseObject(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>& callback,
                 nlohmann::json& body) {
    if (req->getBody().empty()) {
        callback(http_util::error(drogon::k400BadRequest, "body is required"));
        return false;
    }
    try {
        body = nlohmann::json::parse(req->getBody());
    } catch (const std::exception&) {
        callback(http_util::error(drogon::k400BadRequest, "invalid JSON body"));
        return false;
    }
    if (!body.is_object()) {
        callback(http_util::error(drogon::k400BadRequest,
                                  "body must be a JSON object"));
        return false;
    }
    return true;
}

// 取路径参数 {space_id}（registerHandler lambda 路由下在位置向量里）。
std::string routeSpaceId(const drogon::HttpRequestPtr& req) {
    const auto& params = req->getRoutingParameters();
    return params.empty() ? std::string() : params[0];
}

}  // namespace

SpaceController::SpaceController(std::shared_ptr<SpaceService> service)
    : service_(std::move(service)) {}

void SpaceController::spaces(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);
    const auto result = service_->list(id.user_id, id.role);
    callback(http_util::jsonResponse(result.status, result.body));
}

void SpaceController::adminList(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);
    const auto result = service_->adminList(id.role);
    callback(http_util::jsonResponse(result.status, result.body));
}

void SpaceController::create(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);

    nlohmann::json body;
    if (!parseObject(req, callback, body)) {
        return;
    }
    if (!body.contains("name") || !body["name"].is_string() ||
        !body.contains("type") || !body["type"].is_string()) {
        callback(http_util::error(drogon::k400BadRequest,
                                  "name and type must be strings"));
        return;
    }
    if (body.contains("space_id") && !body["space_id"].is_string()) {
        callback(http_util::error(drogon::k400BadRequest,
                                  "space_id must be a string"));
        return;
    }

    const auto result = service_->create(id.role,
                                         body["name"].get<std::string>(),
                                         body["type"].get<std::string>(),
                                         body.value("space_id", ""));
    callback(http_util::jsonResponse(result.status, result.body));
}

void SpaceController::update(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);
    const std::string spaceId = routeSpaceId(req);
    if (spaceId.empty()) {
        callback(http_util::error(drogon::k400BadRequest, "space_id required"));
        return;
    }

    nlohmann::json body;
    if (!parseObject(req, callback, body)) {
        return;
    }

    std::optional<std::string> name, type;
    if (body.contains("name")) {
        if (!body["name"].is_string()) {
            callback(http_util::error(drogon::k400BadRequest,
                                      "name must be a string"));
            return;
        }
        name = body["name"].get<std::string>();
    }
    if (body.contains("type")) {
        if (!body["type"].is_string()) {
            callback(http_util::error(drogon::k400BadRequest,
                                      "type must be a string"));
            return;
        }
        type = body["type"].get<std::string>();
    }

    const auto result = service_->update(id.role, spaceId, name, type);
    callback(http_util::jsonResponse(result.status, result.body));
}

void SpaceController::disable(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);
    const std::string spaceId = routeSpaceId(req);
    if (spaceId.empty()) {
        callback(http_util::error(drogon::k400BadRequest, "space_id required"));
        return;
    }
    const auto result = service_->disable(id.role, spaceId);
    callback(http_util::jsonResponse(result.status, result.body));
}
