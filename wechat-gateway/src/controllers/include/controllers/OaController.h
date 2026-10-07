#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

#include <functional>
#include <memory>

#include "services/OaBindService.h"
#include "services/OaNotifyService.h"

class OaController {
public:
    explicit OaController(std::shared_ptr<OaNotifyService> service,
                          std::shared_ptr<OaBindService> bindService);

    // POST /internal/oa/arrival-notify（设计文档 十一 到校推送）。
    void arrivalNotify(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // GET /api/oa/bind/authorize?code=xxx（设计文档 十一 绑定流程）。
    void bindAuthorize(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // POST /api/oa/bind/send-code（设计文档 十一 绑定流程）。
    void bindSendCode(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // POST /api/oa/bind/confirm（设计文档 十一 绑定流程）。
    void bindConfirm(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // GET /api/oa/bind/me?openid_token=（设计文档 十一 绑定管理）。
    void bindMe(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // POST /api/oa/bind/unbind（设计文档 十一 绑定管理）。
    void bindUnbind(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    std::shared_ptr<OaNotifyService> service_;
    std::shared_ptr<OaBindService> bindService_;
};
