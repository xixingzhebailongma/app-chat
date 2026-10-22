#include "controllers/AiController.h"

#include <nlohmann/json.hpp>

#include <string>
#include <utility>
#include <vector>

#include "utils/HttpResponseUtil.h"
#include "utils/RequestUtil.h"

AiController::AiController(std::shared_ptr<AiService> service)
    : service_(std::move(service)) {}

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

    // 只保留 user/assistant 文本；system/tool 由网关自己注入，不接受客户端塞入
    // （防提示注入）。限制最近 N 条防止上下文超限。
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

    const auto result = service_->chat(id.user_id, id.role, jwt, messages);
    if (result.contains("error")) {
        callback(http_util::error(drogon::k502BadGateway,
                                  result["error"].get<std::string>()));
        return;
    }
    callback(http_util::jsonResponse(drogon::k200OK, result));
}
