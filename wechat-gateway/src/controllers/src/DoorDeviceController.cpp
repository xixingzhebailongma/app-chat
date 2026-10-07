#include "controllers/DoorDeviceController.h"

#include <nlohmann/json.hpp>

#include <string>
#include <utility>

#include "utils/HttpResponseUtil.h"
#include "utils/RequestUtil.h"

DoorDeviceController::DoorDeviceController(
    std::shared_ptr<DoorDeviceService> service)
    : service_(std::move(service)) {}

void DoorDeviceController::list(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);
    const std::string spaceId = req->getParameter("space_id");
    const auto result = service_->list(id.user_id, id.role, spaceId);
    callback(http_util::jsonResponse(result.status, result.body));
}

void DoorDeviceController::mark(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);

    std::string deviceId, spaceId, label;
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
            (body.contains("device_id") && !body["device_id"].is_string()) ||
            (body.contains("space_id") && !body["space_id"].is_string()) ||
            (body.contains("label") && !body["label"].is_string())) {
            callback(http_util::error(drogon::k400BadRequest,
                                      "device_id/space_id/label must be strings"));
            return;
        }
        deviceId = body.value("device_id", "");
        spaceId = body.value("space_id", "");
        label = body.value("label", "");
    }

    const auto result =
        service_->mark(id.user_id, id.role, deviceId, spaceId, label);
    callback(http_util::jsonResponse(result.status, result.body));
}

void DoorDeviceController::unmark(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);
    const std::string spaceId = req->getParameter("space_id");
    const std::string deviceId = req->getParameter("device_id");
    const auto result =
        service_->unmark(id.user_id, id.role, spaceId, deviceId);
    callback(http_util::jsonResponse(result.status, result.body));
}
