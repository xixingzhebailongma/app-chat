#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

#include <functional>
#include <memory>

#include "services/SpaceService.h"

class SpaceController {
public:
    explicit SpaceController(std::shared_ptr<SpaceService> service);

    // GET /api/miniapp/spaces
    void spaces(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    // GET /api/admin/spaces
    void adminList(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    // POST /api/admin/spaces
    void create(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    // PUT /api/admin/spaces/{space_id}
    void update(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    // DELETE /api/admin/spaces/{space_id}
    void disable(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    std::shared_ptr<SpaceService> service_;
};
