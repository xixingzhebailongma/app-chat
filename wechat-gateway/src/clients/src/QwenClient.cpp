#include "clients/QwenClient.h"

#include <drogon/HttpClient.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpTypes.h>

#include <utility>

QwenClient::QwenClient(std::string baseUrl, std::string apiKey, std::string model,
                       double timeoutSeconds)
    : baseUrl_(std::move(baseUrl)),
      apiKey_(std::move(apiKey)),
      model_(std::move(model)),
      timeoutSeconds_(timeoutSeconds) {}

LLMChatResult QwenClient::chat(const std::vector<nlohmann::json>& messages,
                               const std::vector<nlohmann::json>& tools,
                               int maxTokens, bool enableThinking) const {
    LLMChatResult r;

    nlohmann::json body;
    body["model"] = model_;
    body["messages"] = messages;
    if (!tools.empty()) {
        body["tools"] = tools;
        body["tool_choice"] = "auto";
    }
    body["temperature"] = 0.0;  // 工具路由/结构化：确定性优先
    body["max_tokens"] = maxTokens;
    if (!enableThinking) {
        // llama.cpp 专有：关闭思考可显著降延迟（对方文档实测 ~0.4s vs ~7.5s）。
        body["chat_template_kwargs"] =
            nlohmann::json{{"enable_thinking", false}};
    }

    auto client = drogon::HttpClient::newHttpClient(baseUrl_);
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath("/chat/completions");
    req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    req->setBody(body.dump());
    if (!apiKey_.empty()) {
        req->addHeader("Authorization", "Bearer " + apiKey_);
    }

    const auto [result, resp] = client->sendRequest(req, timeoutSeconds_);
    if (result != drogon::ReqResult::Ok || !resp) {
        r.error = "llm unreachable";
        return r;
    }
    if (resp->getStatusCode() != drogon::k200OK) {
        r.error = "llm http " + std::to_string(resp->getStatusCode());
        return r;
    }

    const auto j = nlohmann::json::parse(resp->getBody(), nullptr, false);
    if (j.is_discarded() || !j.is_object() || !j.contains("choices") ||
        !j["choices"].is_array() || j["choices"].empty()) {
        r.error = "llm response malformed";
        return r;
    }

    const auto& choice = j["choices"][0];
    r.finishReason = choice.value("finish_reason", "");
    const auto& msg = choice.value("message", nlohmann::json::object());
    if (msg.contains("content") && msg["content"].is_string()) {
        r.content = msg["content"].get<std::string>();
    }
    if (msg.contains("tool_calls") && msg["tool_calls"].is_array()) {
        for (const auto& tc : msg["tool_calls"]) {
            LLMToolCall c;
            c.id = tc.value("id", "");
            const auto& fn = tc.value("function", nlohmann::json::object());
            c.name = fn.value("name", "");
            c.arguments = fn.value("arguments", "");
            r.toolCalls.push_back(std::move(c));
        }
    }
    r.ok = true;
    return r;
}
