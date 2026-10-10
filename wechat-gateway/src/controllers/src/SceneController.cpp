#include "controllers/SceneController.h"

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

// 取路径参数 {scene_id}（registerHandler lambda 路由下在位置向量里）。
std::string routeSceneId(const drogon::HttpRequestPtr& req) {
    const auto& params = req->getRoutingParameters();
    return params.empty() ? std::string() : params[0];
}

}  // namespace

SceneController::SceneController(std::shared_ptr<SceneService> service)
    : service_(std::move(service)) {}

void SceneController::list(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);
    const std::string spaceId = req->getParameter("space_id");
    if (spaceId.empty()) {
        callback(http_util::error(drogon::k400BadRequest, "space_id required"));
        return;
    }
    const auto r = service_->list(id.user_id, id.role, spaceId);
    callback(http_util::jsonResponse(r.status, r.body));
}

void SceneController::create(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);

    nlohmann::json body;
    if (!parseObject(req, callback, body)) {
        return;
    }
    if (!body.contains("space_id") || !body["space_id"].is_string() ||
        !body.contains("name") || !body["name"].is_string() ||
        !body.contains("device_states") || !body["device_states"].is_array()) {
        callback(http_util::error(
            drogon::k400BadRequest,
            "space_id/name/device_states are required"));
        return;
    }

    const std::string deviceStates = body["device_states"].dump();
    const auto r = service_->create(id.user_id, id.role,
                                    body["space_id"].get<std::string>(),
                                    body["name"].get<std::string>(),
                                    deviceStates);
    callback(http_util::jsonResponse(r.status, r.body));
}

void SceneController::update(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);
    const std::string sceneId = routeSceneId(req);
    if (sceneId.empty()) {
        callback(http_util::error(drogon::k400BadRequest, "scene_id required"));
        return;
    }

    nlohmann::json body;
    if (!parseObject(req, callback, body)) {
        return;
    }

    std::optional<std::string> name, deviceStates;
    if (body.contains("name")) {
        if (!body["name"].is_string()) {
            callback(http_util::error(drogon::k400BadRequest,
                                      "name must be a string"));
            return;
        }
        name = body["name"].get<std::string>();
    }
    if (body.contains("device_states")) {
        if (!body["device_states"].is_array()) {
            callback(http_util::error(drogon::k400BadRequest,
                                      "device_states must be an array"));
            return;
        }
        deviceStates = body["device_states"].dump();
    }

    const auto r = service_->update(id.user_id, id.role, sceneId, name,
                                    deviceStates);
    callback(http_util::jsonResponse(r.status, r.body));
}

void SceneController::remove(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);
    const std::string sceneId = routeSceneId(req);
    if (sceneId.empty()) {
        callback(http_util::error(drogon::k400BadRequest, "scene_id required"));
        return;
    }
    const auto r = service_->remove(id.user_id, id.role, sceneId);
    callback(http_util::jsonResponse(r.status, r.body));
}

void SceneController::execute(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);
    const std::string jwt = request_util::bearerToken(req);
    const std::string sceneId = routeSceneId(req);
    if (sceneId.empty()) {
        callback(http_util::error(drogon::k400BadRequest, "scene_id required"));
        return;
    }

    service_->execute(id.user_id, id.role, jwt, sceneId,
                      [callback = std::move(callback)](const SceneResult& r) {
                          callback(http_util::passthrough(
                              r.status, r.contentType, r.body));
                      });
}
