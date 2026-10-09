#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

#include <functional>
#include <memory>

#include "services/SpaceTypeService.h"

class SpaceTypeController {
public:
    explicit SpaceTypeController(std::shared_ptr<SpaceTypeService> service);

    // GET /api/miniapp/space-types
    void list(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    // POST /api/admin/space-types
    void create(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    // PUT /api/admin/space-types/{code}
    void update(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    // DELETE /api/admin/space-types/{code}
    void disable(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    // PATCH /api/admin/space-types/order
    void reorder(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    std::shared_ptr<SpaceTypeService> service_;
};
