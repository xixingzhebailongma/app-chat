#include "controllers/OaController.h"

#include <nlohmann/json.hpp>
#include <thread>
#include <utility>

#include "dto/ArrivalNotifyDto.h"
#include "dto/OaBindDto.h"
#include "utils/HttpResponseUtil.h"

OaController::OaController(std::shared_ptr<OaNotifyService> service,
                           std::shared_ptr<OaBindService> bindService)
    : service_(std::move(service)), bindService_(std::move(bindService)) {}

void OaController::arrivalNotify(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    nlohmann::json body;
    try {
        body = nlohmann::json::parse(req->getBody());
    } catch (const std::exception&) {
        callback(http_util::error(drogon::k400BadRequest, "invalid JSON body"));
        return;
    }

    ArrivalNotifyRequest dto;
    std::string err;
    if (!ArrivalNotifyRequest::fromJson(body, dto, err)) {
        callback(http_util::error(drogon::k400BadRequest, err));
        return;
    }

    // wechat_oa 真实发送会同步访问微信（access_token 缓存/模板消息），且
    // drogon 的同步 HttpClient 禁止在事件循环线程上运行；通知分发也会做同步
    // DB 查询。与 NotifyController::send / SubscribeNotifyController::send 相同，
    // 在后台线程执行后回写响应，避免阻塞事件循环 / 触发 HttpClient 断言。
    auto service = service_;
    std::thread(
        [service, dto = std::move(dto), cb = std::move(callback)]() {
            ArrivalNotifyResponse resp = service->notify(dto);
            const int status = arrivalNotifyHttpStatus(resp);
            if (status != 200) {
                if (resp.config_error) {
                    cb(http_util::errorWithCode(
                        drogon::k503ServiceUnavailable, "CHANNEL_NOT_CONFIGURED",
                        "wechat_oa arrival template not configured"));
                } else {
                    cb(http_util::error(drogon::k503ServiceUnavailable,
                                        "database unavailable"));
                }
                return;
            }
            cb(http_util::ok(resp.toJson()));
        })
        .detach();
}

void OaController::bindAuthorize(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const std::string code = req->getParameter("code");
    if (code.empty()) {
        callback(http_util::error(drogon::k400BadRequest, "code is required"));
        return;
    }
    if (!bindService_) {
        callback(http_util::error(drogon::k500InternalServerError,
                                  "oa bind service unavailable"));
        return;
    }

    bindService_->authorize(
        code, [callback = std::move(callback)](const OaBindResult& r) {
            callback(http_util::jsonResponse(r.status, r.body));
        });
}

void OaController::bindSendCode(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    nlohmann::json body;
    try {
        body = nlohmann::json::parse(req->getBody());
    } catch (const std::exception&) {
        callback(http_util::error(drogon::k400BadRequest, "invalid JSON body"));
        return;
    }

    OaSendCodeRequest dto;
    std::string err;
    if (!OaSendCodeRequest::fromJson(body, dto, err)) {
        callback(http_util::error(drogon::k400BadRequest, err));
        return;
    }
    if (!bindService_) {
        callback(http_util::error(drogon::k500InternalServerError,
                                  "oa bind service unavailable"));
        return;
    }

    // 真实短信发送是同步阻塞 HTTP（provider 为阻塞 sendRequest），绝不能在
    // Drogon 事件循环线程上执行——与 NotifyController::send 相同地卸载到后台
    // 线程后回写响应。bindService_ 为 shared_ptr，随线程存活。
    auto bindService = bindService_;
    const std::string phone = dto.phone;
    std::thread(
        [bindService, phone, callback = std::move(callback)]() mutable {
            bindService->sendCode(
                phone,
                [cb = std::move(callback)](const OaBindResult& r) {
                    cb(http_util::jsonResponse(r.status, r.body));
                });
        })
        .detach();
}

void OaController::bindConfirm(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    nlohmann::json body;
    try {
        body = nlohmann::json::parse(req->getBody());
    } catch (const std::exception&) {
        callback(http_util::error(drogon::k400BadRequest, "invalid JSON body"));
        return;
    }

    OaBindConfirmRequest dto;
    std::string err;
    if (!OaBindConfirmRequest::fromJson(body, dto, err)) {
        callback(http_util::error(drogon::k400BadRequest, err));
        return;
    }
    if (!bindService_) {
        callback(http_util::error(drogon::k500InternalServerError,
                                  "oa bind service unavailable"));
        return;
    }

    bindService_->confirm(
        dto.openid_token, dto.student_no, dto.phone, dto.sms_code,
        [callback = std::move(callback)](const OaBindResult& r) {
            callback(http_util::jsonResponse(r.status, r.body));
        });
}

void OaController::bindMe(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const std::string token = req->getParameter("openid_token");
    if (token.empty()) {
        callback(http_util::error(drogon::k400BadRequest,
                                  "openid_token is required"));
        return;
    }
    if (!bindService_) {
        callback(http_util::error(drogon::k500InternalServerError,
                                  "oa bind service unavailable"));
        return;
    }

    bindService_->me(
        token, [callback = std::move(callback)](const OaBindResult& r) {
            callback(http_util::jsonResponse(r.status, r.body));
        });
}

void OaController::bindUnbind(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    nlohmann::json body;
    try {
        body = nlohmann::json::parse(req->getBody());
    } catch (const std::exception&) {
        callback(http_util::error(drogon::k400BadRequest, "invalid JSON body"));
        return;
    }

    OaUnbindRequest dto;
    std::string err;
    if (!OaUnbindRequest::fromJson(body, dto, err)) {
        callback(http_util::error(drogon::k400BadRequest, err));
        return;
    }
    if (!bindService_) {
        callback(http_util::error(drogon::k500InternalServerError,
                                  "oa bind service unavailable"));
        return;
    }

    bindService_->unbind(
        dto.openid_token, dto.student_no,
        [callback = std::move(callback)](const OaBindResult& r) {
            callback(http_util::jsonResponse(r.status, r.body));
        });
}
