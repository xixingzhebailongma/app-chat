#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

#include <functional>
#include <memory>

#include "services/AiConversationService.h"
#include "services/AiService.h"

class AiController {
public:
    AiController(std::shared_ptr<AiService> service,
                 std::shared_ptr<AiConversationService> conversations);

    // POST /api/ai/chat（JwtFilter 鉴权）
    void chat(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // GET /api/ai/conversations（列出本人会话）
    void listConversations(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // GET /api/ai/conversations/{conversation_id}/messages（取历史）
    void listMessages(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // DELETE /api/ai/conversations/{conversation_id}（删除本人会话）
    void remove(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    std::shared_ptr<AiService> service_;
    std::shared_ptr<AiConversationService> conversations_;
};
