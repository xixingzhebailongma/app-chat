#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

#include <functional>
#include <memory>

#include "services/SubscriptionService.h"

class SubscriptionController {
public:
    explicit SubscriptionController(std::shared_ptr<SubscriptionService> service);

    // POST /api/miniapp/subscribe —— 上报授权（accepted_templates）或关闭订阅
    // （unsubscribe_all）。
    void subscribe(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // GET /api/miniapp/subscribe —— 回显当前各模板订阅状态。
    void get(const drogon::HttpRequestPtr& req,
             std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // GET /api/miniapp/notify/templates —— 返回去重后的模板 ID 集合。
    void templates(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    std::shared_ptr<SubscriptionService> service_;
};
