#include "controllers/NotifyBindingController.h"

#include <nlohmann/json.hpp>

#include <utility>

#include "utils/HttpResponseUtil.h"
#include "utils/RequestUtil.h"

NotifyBindingController::NotifyBindingController(
    std::shared_ptr<NotifyBindingService> service)
    : service_(std::move(service)) {}

void NotifyBindingController::list(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);
    const auto r = service_->list(id.role, req->getParameter("channel"));
    callback(http_util::jsonResponse(r.status, r.body));
}

void NotifyBindingController::save(
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
    if (!body.is_object() || !body.contains("channel") ||
        !body["channel"].is_string() || !body.contains("user_id") ||
        !body["user_id"].is_string() || !body.contains("external_id") ||
        !body["external_id"].is_string()) {
        callback(http_util::error(drogon::k400BadRequest,
                                  "channel, user_id and external_id must be "
                                  "strings"));
        return;
    }
    std::string externalExtra;
    if (body.contains("external_extra") && body["external_extra"].is_string()) {
        externalExtra = body["external_extra"].get<std::string>();
    }

    const auto r = service_->save(
        id.role, body["channel"].get<std::string>(),
        body["user_id"].get<std::string>(),
        body["external_id"].get<std::string>(), externalExtra);
    callback(http_util::jsonResponse(r.status, r.body));
}

void NotifyBindingController::remove(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);
    const auto r = service_->remove(id.role, req->getParameter("channel"),
                                    req->getParameter("user_id"));
    callback(http_util::jsonResponse(r.status, r.body));
}
