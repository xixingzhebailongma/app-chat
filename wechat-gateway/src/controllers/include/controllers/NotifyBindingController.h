#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

#include <functional>
#include <memory>

#include "services/NotifyBindingService.h"

class NotifyBindingController {
public:
    explicit NotifyBindingController(std::shared_ptr<NotifyBindingService> service);

    // GET /api/notify/bindings?channel=
    void list(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    // POST /api/notify/bindings
    void save(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    // DELETE /api/notify/bindings?channel=&user_id=
    void remove(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    std::shared_ptr<NotifyBindingService> service_;
};
