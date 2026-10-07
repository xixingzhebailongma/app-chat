#include "controllers/SubscribeNotifyController.h"

#include <nlohmann/json.hpp>

#include <thread>
#include <utility>

#include "dto/SubscribeSendDto.h"
#include "utils/HttpResponseUtil.h"

SubscribeNotifyController::SubscribeNotifyController(
    std::shared_ptr<WechatMiniAppChannel> channel)
    : channel_(std::move(channel)) {}

void SubscribeNotifyController::send(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    nlohmann::json body;
    try {
        body = nlohmann::json::parse(req->getBody());
    } catch (const std::exception&) {
        callback(http_util::error(drogon::k400BadRequest, "invalid JSON body"));
        return;
    }

    SubscribeSendRequest dto;
    std::string err;
    if (!SubscribeSendRequest::fromJson(body, dto, err)) {
        callback(http_util::error(drogon::k400BadRequest, err));
        return;
    }

    // 微信同步 HTTP 调用绝不能在 Drogon 事件循环线程上执行（HttpClient
    // 同步 sendRequest 会断言 isInLoopThread）；在后台线程发送后回写响应。
    // 直发路径不经 NotifyService、不消耗订阅额度，用 sendDirect。
    auto channel = channel_;
    std::thread(
        [channel, dto = std::move(dto), cb = std::move(callback)]() {
            const ChannelResult r = channel->sendDirect(
                dto.openid, dto.template_id, dto.page, dto.miniprogram_state,
                dto.data);
            cb(http_util::ok(nlohmann::json{{"success", r.success},
                                            {"channel", r.channel},
                                            {"errcode", r.errcode},
                                            {"errmsg", r.errmsg}}));
        })
        .detach();
}
