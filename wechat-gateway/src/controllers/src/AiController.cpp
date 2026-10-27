#include "controllers/AiController.h"

#include <nlohmann/json.hpp>

#include <string>
#include <utility>
#include <vector>

#include "utils/HttpResponseUtil.h"
#include "utils/RequestUtil.h"

namespace {

// 取路径参数 {conversation_id}（registerHandler lambda 路由下在位置向量里）。
std::string routeConversationId(const drogon::HttpRequestPtr& req) {
    const auto& params = req->getRoutingParameters();
    return params.empty() ? std::string() : params[0];
}

}  // namespace

AiController::AiController(std::shared_ptr<AiService> service,
                           std::shared_ptr<AiConversationService> conversations)
    : service_(std::move(service)), conversations_(std::move(conversations)) {}

void AiController::chat(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);
    const std::string jwt = request_util::bearerToken(req);

    nlohmann::json body;
    try {
        body = nlohmann::json::parse(req->getBody());
    } catch (const std::exception&) {
        callback(http_util::error(drogon::k400BadRequest, "invalid JSON body"));
        return;
    }
    if (!body.is_object() || !body.contains("messages") ||
        !body["messages"].is_array()) {
        callback(http_util::error(drogon::k400BadRequest,
                                  "messages must be an array"));
        return;
    }

    // 可选会话 id：缺省 = 新建会话；非 string 报 400。
    std::string conversationId;
    if (body.contains("conversation_id")) {
        if (!body["conversation_id"].is_string()) {
            callback(http_util::error(drogon::k400BadRequest,
                                      "conversation_id must be a string"));
            return;
        }
        conversationId = body["conversation_id"].get<std::string>();
    }

    // 只保留 user/assistant 文本；system/tool 由网关自己注入，不接受客户端塞入
    // （防提示注入）。限制最近 N 条防止上下文超限（防御性，服务端另有截断）。
    std::vector<nlohmann::json> messages;
    const auto& in = body["messages"];
    constexpr size_t kMaxMsgs = 20;
    const size_t start = in.size() > kMaxMsgs ? in.size() - kMaxMsgs : 0;
    for (size_t i = start; i < in.size(); ++i) {
        const auto& m = in[i];
        if (!m.is_object() || !m.contains("role") || !m["role"].is_string() ||
            !m.contains("content") || !m["content"].is_string()) {
            continue;
        }
        const std::string role = m["role"].get<std::string>();
        if (role != "user" && role != "assistant") {
            continue;
        }
        messages.push_back({{"role", role}, {"content", m["content"]}});
    }
    if (messages.empty()) {
        callback(http_util::error(drogon::k400BadRequest, "no valid messages"));
        return;
    }

    const auto result =
        service_->chat(id.user_id, id.role, jwt, messages, conversationId);
    if (result.contains("error")) {
        const int status = result.value("status", 502);
        callback(http_util::error(static_cast<drogon::HttpStatusCode>(status),
                                  result["error"].get<std::string>()));
        return;
    }
    callback(http_util::jsonResponse(drogon::k200OK, result));
}

void AiController::listConversations(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);
    const auto convs = conversations_->list(id.user_id);
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& c : convs) {
        arr.push_back({{"conversation_id", c.conversation_id},
                       {"title", c.title},
                       {"created_at", c.created_at},
                       {"updated_at", c.updated_at}});
    }
    callback(http_util::jsonResponse(drogon::k200OK, {{"conversations", arr}}));
}

void AiController::listMessages(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);
    const std::string conversationId = routeConversationId(req);
    if (conversationId.empty()) {
        callback(http_util::error(drogon::k400BadRequest,
                                  "conversation_id required"));
        return;
    }
    if (!conversations_->get(id.user_id, conversationId)) {
        callback(http_util::error(drogon::k404NotFound,
                                  "conversation not found"));
        return;
    }
    const auto msgs = conversations_->messages(conversationId, /*limit=*/-1);
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& m : msgs) {
        arr.push_back({{"role", m.role},
                       {"content", m.content},
                       {"created_at", m.created_at}});
    }
    callback(http_util::jsonResponse(
        drogon::k200OK,
        {{"conversation_id", conversationId}, {"messages", arr}}));
}

void AiController::remove(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);
    const std::string conversationId = routeConversationId(req);
    if (conversationId.empty()) {
        callback(http_util::error(drogon::k400BadRequest,
                                  "conversation_id required"));
        return;
    }
    if (!conversations_->remove(id.user_id, conversationId)) {
        callback(http_util::error(drogon::k404NotFound,
                                  "conversation not found"));
        return;
    }
    callback(http_util::jsonResponse(
        drogon::k200OK,
        {{"conversation_id", conversationId}, {"deleted", true}}));
}
