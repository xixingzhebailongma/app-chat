#include "controllers/MiniAppController.h"

#include <utility>

#include "dto/DeviceControlDto.h"
#include "dto/MiniappAuthDto.h"
#include "utils/HttpResponseUtil.h"
#include "utils/RequestUtil.h"

MiniAppController::MiniAppController(
    std::shared_ptr<AuthService> service,
    std::shared_ptr<DeviceControlService> deviceControl)
    : service_(std::move(service)),
      deviceControl_(std::move(deviceControl)) {}

void MiniAppController::login(const drogon::HttpRequestPtr& req,
                              Callback&& callback) {
    nlohmann::json body;
    try {
        body = nlohmann::json::parse(req->getBody());
    } catch (const std::exception&) {
        callback(http_util::error(drogon::k400BadRequest, "invalid JSON body"));
        return;
    }

    MiniappLoginRequest dto;
    std::string err;
    if (!MiniappLoginRequest::fromJson(body, dto, err)) {
        callback(http_util::error(drogon::k400BadRequest, err));
        return;
    }

    service_->login(dto.code,
                    [callback = std::move(callback)](const AuthResult& r) {
                        callback(http_util::jsonResponse(r.status, r.body));
                    });
}

void MiniAppController::bind(const drogon::HttpRequestPtr& req,
                             Callback&& callback) {
    nlohmann::json body;
    try {
        body = nlohmann::json::parse(req->getBody());
    } catch (const std::exception&) {
        callback(http_util::error(drogon::k400BadRequest, "invalid JSON body"));
        return;
    }

    MiniappBindRequest dto;
    std::string err;
    if (!MiniappBindRequest::fromJson(body, dto, err)) {
        callback(http_util::error(drogon::k400BadRequest, err));
        return;
    }

    service_->bind(dto.openid_token, dto.username, dto.password,
                   [callback = std::move(callback)](const AuthResult& r) {
                       callback(http_util::jsonResponse(r.status, r.body));
                   });
}

void MiniAppController::me(const drogon::HttpRequestPtr& req,
                           Callback&& callback) {
    const auto id = request_util::identity(req);
    callback(http_util::ok(
        nlohmann::json{{"user_id", id.user_id}, {"role", id.role}}));
}

void MiniAppController::deviceControl(const drogon::HttpRequestPtr& req,
                                      Callback&& callback) {
    nlohmann::json body;
    try {
        body = nlohmann::json::parse(req->getBody());
    } catch (const std::exception&) {
        callback(http_util::error(drogon::k400BadRequest, "invalid JSON body"));
        return;
    }

    DeviceControlRequest dto;
    std::string err;
    if (!DeviceControlRequest::fromJson(body, dto, err)) {
        callback(http_util::error(drogon::k400BadRequest, err));
        return;
    }

    // 身份信息由 JwtFilter 暂存（已验证）；调用方的
    // 原始 bearer 令牌原样转发给 go-backend。
    const auto id = request_util::identity(req);
    const std::string jwt = request_util::bearerToken(req);

    deviceControl_->control(
        id.user_id, id.role, jwt, dto.space_id, dto.device_type, dto.device_id,
        dto.command, dto.confirm,
        [callback = std::move(callback)](const DeviceControlResult& r) {
            callback(http_util::passthrough(r.status, r.contentType, r.body));
        });
}
