#include "controllers/SpaceTypeController.h"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <utility>
#include <vector>

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

}  // namespace

SpaceTypeController::SpaceTypeController(
    std::shared_ptr<SpaceTypeService> service)
    : service_(std::move(service)) {}

void SpaceTypeController::list(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto r = service_->list();
    callback(http_util::jsonResponse(r.status, r.body));
}

void SpaceTypeController::create(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);

    nlohmann::json body;
    if (!parseObject(req, callback, body)) {
        return;
    }
    if (!body.contains("code") || !body["code"].is_string() ||
        !body.contains("name") || !body["name"].is_string()) {
        callback(http_util::error(drogon::k400BadRequest,
                                  "code and name must be strings"));
        return;
    }
    if (body.contains("icon") && !body["icon"].is_string()) {
        callback(http_util::error(drogon::k400BadRequest,
                                  "icon must be a string"));
        return;
    }
    std::optional<int> sortOrder;
    if (body.contains("sort_order")) {
        if (!body["sort_order"].is_number_integer()) {
            callback(http_util::error(drogon::k400BadRequest,
                                      "sort_order must be an integer"));
            return;
        }
        sortOrder = body["sort_order"].get<int>();
    }

    const auto r = service_->create(id.role,
                                    body["code"].get<std::string>(),
                                    body["name"].get<std::string>(),
                                    body.value("icon", ""), sortOrder);
    callback(http_util::jsonResponse(r.status, r.body));
}

void SpaceTypeController::update(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);
    // 路径参数 {code} 在 registerHandler（lambda）路由下落在
    // getRoutingParameters()（位置向量），而非 getParameter()（只含 query/form）。
    const auto& routingParams = req->getRoutingParameters();
    const std::string code =
        routingParams.empty() ? std::string() : routingParams[0];
    if (code.empty()) {
        callback(http_util::error(drogon::k400BadRequest, "code required"));
        return;
    }

    nlohmann::json body;
    if (!parseObject(req, callback, body)) {
        return;
    }

    std::optional<std::string> name, icon;
    std::optional<bool> enabled;
    if (body.contains("name")) {
        if (!body["name"].is_string()) {
            callback(http_util::error(drogon::k400BadRequest,
                                      "name must be a string"));
            return;
        }
        name = body["name"].get<std::string>();
    }
    if (body.contains("icon")) {
        if (!body["icon"].is_string()) {
            callback(http_util::error(drogon::k400BadRequest,
                                      "icon must be a string"));
            return;
        }
        icon = body["icon"].get<std::string>();
    }
    if (body.contains("enabled")) {
        if (!body["enabled"].is_boolean()) {
            callback(http_util::error(drogon::k400BadRequest,
                                      "enabled must be a boolean"));
            return;
        }
        enabled = body["enabled"].get<bool>();
    }

    const auto r = service_->update(id.role, code, name, icon, enabled);
    callback(http_util::jsonResponse(r.status, r.body));
}

void SpaceTypeController::disable(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);
    // 路径参数 {code} 与 update() 一致：registerHandler（lambda）路由下落在
    // getRoutingParameters()（位置向量），而非 getParameter()（只含 query/form）。
    const auto& routingParams = req->getRoutingParameters();
    const std::string code =
        routingParams.empty() ? std::string() : routingParams[0];
    if (code.empty()) {
        callback(http_util::error(drogon::k400BadRequest, "code required"));
        return;
    }
    const auto r = service_->disable(id.role, code);
    callback(http_util::jsonResponse(r.status, r.body));
}

void SpaceTypeController::reorder(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);

    nlohmann::json body;
    if (!parseObject(req, callback, body)) {
        return;
    }
    if (!body.contains("codes") || !body["codes"].is_array()) {
        callback(http_util::error(drogon::k400BadRequest,
                                  "codes must be an array"));
        return;
    }
    std::vector<std::string> codes;
    codes.reserve(body["codes"].size());
    for (const auto& c : body["codes"]) {
        if (!c.is_string()) {
            callback(http_util::error(drogon::k400BadRequest,
                                      "codes must be strings"));
            return;
        }
        codes.push_back(c.get<std::string>());
    }

    const auto r = service_->reorder(id.role, codes);
    callback(http_util::jsonResponse(r.status, r.body));
}
