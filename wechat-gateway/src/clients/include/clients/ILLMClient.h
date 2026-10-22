#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

// 一次工具调用（OpenAI function-calling 契约）：arguments 是 JSON 字符串，
// 需调用方二次解析并容忍模型产出非法 JSON。
struct LLMToolCall {
    std::string id;
    std::string name;
    std::string arguments;
};

struct LLMChatResult {
    bool ok = false;  // 传输失败 / 非 200 / 响应格式非法时为 false
    std::string content;      // finish_reason=stop 时的文本
    std::vector<LLMToolCall> toolCalls;
    std::string finishReason;  // stop / tool_calls / length
    std::string error;
};

// LLM 客户端抽象：AiService 只依赖该接口，便于用假实现单测函数调用循环
// （不触网）。生产实现为 QwenClient（本地 llama.cpp，OpenAI 兼容）。
class ILLMClient {
public:
    virtual ~ILLMClient() = default;

    // messages 已含 system；tools 为空数组表示纯对话。
    virtual LLMChatResult chat(const std::vector<nlohmann::json>& messages,
                               const std::vector<nlohmann::json>& tools,
                               int maxTokens, bool enableThinking) const = 0;
};
