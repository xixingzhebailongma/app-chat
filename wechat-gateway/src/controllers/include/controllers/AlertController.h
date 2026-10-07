#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

#include <functional>
#include <memory>

#include "services/AlertService.h"

class AlertController {
public:
    explicit AlertController(std::shared_ptr<AlertService> service);

    // GET /api/miniapp/alerts
    void list(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // POST /api/miniapp/alerts/{id}/handle
    void handle(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // GET /api/miniapp/alerts/{id}/timeline
    void timeline(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // GET /api/miniapp/alerts/{id}
    void detail(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // GET /api/miniapp/alerts/stats
    void stats(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    std::shared_ptr<AlertService> service_;
};
