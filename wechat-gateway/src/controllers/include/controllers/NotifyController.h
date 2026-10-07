#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

#include <functional>
#include <memory>

#include "services/NotifyService.h"

class NotifyController {
public:
    explicit NotifyController(std::shared_ptr<NotifyService> service);

    void send(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    std::shared_ptr<NotifyService> service_;
};
