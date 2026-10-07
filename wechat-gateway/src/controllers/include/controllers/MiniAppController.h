#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

#include <functional>
#include <memory>

#include "services/AuthService.h"
#include "services/DeviceControlService.h"
#include "services/SceneService.h"

class MiniAppController {
public:
    using Callback =
        std::function<void(const drogon::HttpResponsePtr&)>;

    explicit MiniAppController(std::shared_ptr<AuthService> service,
                               std::shared_ptr<DeviceControlService> deviceControl,
                               std::shared_ptr<SceneService> scene);

    void login(const drogon::HttpRequestPtr& req, Callback&& callback);
    void bind(const drogon::HttpRequestPtr& req, Callback&& callback);

    // GET /api/miniapp/me —— 由 JwtFilter 认证；返回从 bearer 令牌解码出的调用方
    // 身份（六 中的“后续请求”路径）。
    void me(const drogon::HttpRequestPtr& req, Callback&& callback);

    // POST /api/miniapp/device/control —— 在角色/空间授权和高风险确认门
    // 之后，将设备命令转发给 go-backend（设计文档 八）。
    void deviceControl(const drogon::HttpRequestPtr& req, Callback&& callback);

    // POST /api/miniapp/scene/execute —— 一键场景，服务端展开为多条
    // on/off 转发 go-backend（设计文档 7.4②）。
    void sceneExecute(const drogon::HttpRequestPtr& req, Callback&& callback);

private:
    std::shared_ptr<AuthService> service_;
    std::shared_ptr<DeviceControlService> deviceControl_;
    std::shared_ptr<SceneService> scene_;
};
