#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

#include <functional>
#include <memory>

#include "channels/WechatMiniAppChannel.h"

// 微信订阅消息真实发送的最小触发接口控制器。
class SubscribeNotifyController {
public:
    explicit SubscribeNotifyController(
        std::shared_ptr<WechatMiniAppChannel> channel);

    void send(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    std::shared_ptr<WechatMiniAppChannel> channel_;
};
