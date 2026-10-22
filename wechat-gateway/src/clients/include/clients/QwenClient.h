#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

#include "clients/ILLMClient.h"

// 本地 Qwen（llama.cpp llama-server，OpenAI 兼容）客户端。只做同步阻塞
// 请求——供 AiService 在 /api/ai/chat 的 worker 线程上顺序驱动函数调用循环；
// 绝不能在 Drogon 事件循环线程上调用（同 GoBackendClient::getUserPhoneSync）。
class QwenClient : public ILLMClient {
public:
    QwenClient(std::string baseUrl, std::string apiKey, std::string model,
               double timeoutSeconds);

    // enableThinking=false 时通过 chat_template_kwargs 关闭思考（降延迟）。
    LLMChatResult chat(const std::vector<nlohmann::json>& messages,
                       const std::vector<nlohmann::json>& tools, int maxTokens,
                       bool enableThinking) const override;

private:
    std::string baseUrl_;
    std::string apiKey_;
    std::string model_;
    double timeoutSeconds_;
};
