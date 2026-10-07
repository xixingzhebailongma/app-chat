#include "controllers/NotifyController.h"

#include <nlohmann/json.hpp>
#include <thread>
#include <utility>

#include "dto/NotifySendDto.h"
#include "utils/HttpResponseUtil.h"

NotifyController::NotifyController(std::shared_ptr<NotifyService> service)
    : service_(std::move(service)) {}

void NotifyController::send(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    nlohmann::json body;
    try {
        body = nlohmann::json::parse(req->getBody());
    } catch (const std::exception&) {
        callback(http_util::error(drogon::k400BadRequest, "invalid JSON body"));
        return;
    }

    NotifySendRequest dto;
    std::string err;
    if (!NotifySendRequest::fromJson(body, dto, err)) {
        callback(http_util::error(drogon::k400BadRequest, err));
        return;
    }

    // 渠道的真实 send 会做同步 HTTP（小程序订阅消息 / 短信），而 drogon 的
    // 同步 HttpClient 禁止在事件循环线程上运行——在后台线程分发后回写响应
    // （同 SubscribeNotifyController 的写法）。
    auto service = service_;
    std::thread(
        [service, dto = std::move(dto), cb = std::move(callback)]() {
            cb(http_util::ok(service->send(dto).toJson()));
        })
        .detach();
}
