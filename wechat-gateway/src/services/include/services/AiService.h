#pragma once

#include <nlohmann/json.hpp>

#include <memory>
#include <string>
#include <vector>

#include "clients/ILLMClient.h"

class AiTools;

// AI 助手服务：驱动「意图识别 → 工具调用 → 结果回填 → 生成回答」的函数调用
// 循环。同步阻塞，在 /api/ai/chat 的 worker 线程上运行。LLM 只做路由与解释，
// 数据访问与鉴权全部经 AiTools 落到现有服务。
class AiService {
public:
    AiService(std::shared_ptr<ILLMClient> llm, std::shared_ptr<AiTools> tools,
              bool enableThinking, int maxTokens, int maxToolIters);

    // clientMessages：客户端传来的历史（只含 user/assistant 文本，由控制器
    // 过滤）。返回 {"reply", "tool_calls"} 或 {"error"}。
    nlohmann::json chat(const std::string& userId, const std::string& role,
                        const std::string& jwt,
                        const std::vector<nlohmann::json>& clientMessages);

private:
    std::string buildSystemPrompt(const std::string& role,
                                  const std::string& userId) const;

    std::shared_ptr<ILLMClient> llm_;
    std::shared_ptr<AiTools> tools_;
    bool enableThinking_;
    int maxTokens_;
    int maxToolIters_;
};
