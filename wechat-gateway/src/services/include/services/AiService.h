#pragma once

#include <nlohmann/json.hpp>

#include <memory>
#include <string>
#include <vector>

#include "clients/ILLMClient.h"

class AiTools;
class AiConversationService;

// AI 助手服务：驱动「意图识别 → 工具调用 → 结果回填 → 生成回答」的函数调用
// 循环。同步阻塞，在 /api/ai/chat 的 worker 线程上运行。LLM 只做路由与解释，
// 数据访问与鉴权全部经 AiTools 落到现有服务。
//
// conversations 非空时启用服务端会话记忆：按 conversation_id 读最近 N 条历史、
// 落库本轮 user 消息与 assistant 回复；为空（默认）时退化为无状态单轮，
// 行为与接入记忆前一致。
class AiService {
public:
    AiService(std::shared_ptr<ILLMClient> llm, std::shared_ptr<AiTools> tools,
              bool enableThinking, int maxTokens, int maxToolIters,
              std::shared_ptr<AiConversationService> conversations = nullptr);

    // clientMessages：本轮新增消息（只含 user/assistant 文本，由控制器过滤）。
    // conversationId 为空 => 新建会话；非空 => 校验归属后沿用（非本人/不存在
    // 返回 error + status=404）。返回 {"reply","tool_calls","conversation_id"}
    // 或 {"error","status"}。
    nlohmann::json chat(const std::string& userId, const std::string& role,
                        const std::string& jwt,
                        const std::vector<nlohmann::json>& clientMessages,
                        const std::string& conversationId = "");

private:
    std::string buildSystemPrompt(const std::string& role,
                                  const std::string& userId) const;

    std::shared_ptr<ILLMClient> llm_;
    std::shared_ptr<AiTools> tools_;
    std::shared_ptr<AiConversationService> conversations_;
    bool enableThinking_;
    int maxTokens_;
    int maxToolIters_;
};
