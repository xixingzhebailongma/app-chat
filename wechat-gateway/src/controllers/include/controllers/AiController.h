#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

#include <functional>
#include <memory>

#include "services/AiService.h"

class AiController {
public:
    explicit AiController(std::shared_ptr<AiService> service);

    // POST /api/ai/chat（JwtFilter 鉴权）
    void chat(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    std::shared_ptr<AiService> service_;
};
