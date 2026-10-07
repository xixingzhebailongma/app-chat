#include "controllers/AlertController.h"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <string>
#include <utility>

#include "utils/HttpResponseUtil.h"
#include "utils/RequestUtil.h"

namespace {

// 解析非负整数查询参数；空/非法/负数返回 def（分页护栏由服务层再做）。
int parseNonNegativeInt(const std::string& s, int def) {
    if (s.empty()) {
        return def;
    }
    char* end = nullptr;
    const long v = std::strtol(s.c_str(), &end, 10);
    if (end == s.c_str() || *end != '\0' || v < 0) {
        return def;
    }
    return static_cast<int>(v);
}

}  // namespace

AlertController::AlertController(std::shared_ptr<AlertService> service)
    : service_(std::move(service)) {}

void AlertController::list(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);

    // 只做参数解析与响应封装；空间/角色边界与脱敏都在 AlertService 层完成。
    const std::string spaceId = req->getParameter("space_id");
    AlertFilter filter;
    filter.event_type = req->getParameter("event_type");
    filter.status = req->getParameter("status");
    filter.from = req->getParameter("from");
    filter.to = req->getParameter("to");
    const int page = parseNonNegativeInt(req->getParameter("page"), 1);
    const int pageSize = parseNonNegativeInt(req->getParameter("page_size"), 50);

    const auto result =
        service_->list(id.user_id, id.role, spaceId, filter, page, pageSize);
    callback(http_util::jsonResponse(result.status, result.body));
}

void AlertController::handle(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);
    // 路径参数 {id} 在 registerHandler（lambda）路由下落在
    // getRoutingParameters()（位置向量），而非 getParameter()（只含 query/form）。
    const auto& routingParams = req->getRoutingParameters();
    const std::string alertId =
        routingParams.empty() ? std::string() : routingParams[0];

    std::string remark;
    std::string status = "resolved";
    if (!req->getBody().empty()) {
        nlohmann::json body;
        try {
            body = nlohmann::json::parse(req->getBody());
        } catch (const std::exception&) {
            callback(http_util::error(drogon::k400BadRequest,
                                      "invalid JSON body"));
            return;
        }
        if (!body.is_object() ||
            (body.contains("remark") && !body["remark"].is_string()) ||
            (body.contains("status") && !body["status"].is_string())) {
            callback(http_util::error(drogon::k400BadRequest,
                                      "remark and status must be strings"));
            return;
        }
        if (body.contains("remark")) {
            remark = body["remark"].get<std::string>();
        }
        if (body.contains("status")) {
            status = body["status"].get<std::string>();
        }
    }

    const auto result =
        service_->handle(alertId, id.user_id, id.role, remark, status);
    callback(http_util::jsonResponse(result.status, result.body));
}

void AlertController::timeline(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);
    // 路径参数 {id} 与 handle() 一致：registerHandler（lambda）路由下落在
    // getRoutingParameters()（位置向量），而非 getParameter()（只含 query/form）。
    const auto& routingParams = req->getRoutingParameters();
    const std::string alertId =
        routingParams.empty() ? std::string() : routingParams[0];

    const auto result = service_->timeline(alertId, id.role);
    callback(http_util::jsonResponse(result.status, result.body));
}

void AlertController::detail(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);
    // 路径参数 {id} 与 handle()/timeline() 一致：registerHandler（lambda）路由下
    // 落在 getRoutingParameters()（位置向量），而非 getParameter()（只含 query/form）。
    const auto& routingParams = req->getRoutingParameters();
    const std::string alertId =
        routingParams.empty() ? std::string() : routingParams[0];

    const auto result = service_->detail(alertId, id.user_id, id.role);
    callback(http_util::jsonResponse(result.status, result.body));
}

void AlertController::stats(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);
    const auto result = service_->stats(id.user_id, id.role);
    callback(http_util::jsonResponse(result.status, result.body));
}
