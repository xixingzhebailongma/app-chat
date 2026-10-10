#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

#include <functional>
#include <memory>

#include "services/SceneService.h"

class SceneController {
public:
    explicit SceneController(std::shared_ptr<SceneService> service);

    // GET /api/miniapp/scenes?space_id=
    void list(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    // POST /api/miniapp/scenes
    void create(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    // PUT /api/miniapp/scenes/{scene_id}
    void update(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    // DELETE /api/miniapp/scenes/{scene_id}
    void remove(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    // POST /api/miniapp/scenes/{scene_id}/execute
    void execute(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    std::shared_ptr<SceneService> service_;
};
