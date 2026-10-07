#include "controllers/SubscriptionController.h"

#include <nlohmann/json.hpp>

#include <utility>
#include <vector>

#include "utils/HttpResponseUtil.h"
#include "utils/RequestUtil.h"

SubscriptionController::SubscriptionController(
    std::shared_ptr<SubscriptionService> service)
    : service_(std::move(service)) {}

void SubscriptionController::subscribe(
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

    if (!body.is_object()) {
        callback(http_util::error(drogon::k400BadRequest,
                                  "expected a JSON object"));
        return;
    }

    // 关闭订阅：清空全部模板额度。
    if (body.value("unsubscribe_all", false)) {
        const auto r = service_->unsubscribeAll(id.user_id);
        callback(http_util::jsonResponse(r.status, r.body));
        return;
    }

    // 上报授权：只接受 accept 的模板 ID 数组。
    if (!body.contains("accepted_templates") ||
        !body["accepted_templates"].is_array()) {
        callback(http_util::error(
            drogon::k400BadRequest,
            "expected accepted_templates array or unsubscribe_all"));
        return;
    }
    std::vector<std::string> accepted;
    for (const auto& t : body["accepted_templates"]) {
        if (!t.is_string()) {
            callback(http_util::error(drogon::k400BadRequest,
                                      "accepted_templates items must be strings"));
            return;
        }
        accepted.push_back(t.get<std::string>());
    }

    const auto r = service_->applyGrants(id.user_id, accepted);
    callback(http_util::jsonResponse(r.status, r.body));
}

void SubscriptionController::get(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);
    const auto r = service_->state(id.user_id);
    callback(http_util::jsonResponse(r.status, r.body));
}

void SubscriptionController::templates(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    (void)req;
    const auto r = service_->templateIds();
    callback(http_util::jsonResponse(r.status, r.body));
}
