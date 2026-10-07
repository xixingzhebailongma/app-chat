#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

#include <functional>
#include <memory>

#include "services/SpaceBindingService.h"

class SpaceBindingController {
public:
    explicit SpaceBindingController(std::shared_ptr<SpaceBindingService> service);

    // GET /api/miniapp/space-bindings
    void list(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    // POST /api/miniapp/space-bindings
    void bind(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    // DELETE /api/miniapp/space-bindings?user_id=&space_id=
    void unbind(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    std::shared_ptr<SpaceBindingService> service_;
};
