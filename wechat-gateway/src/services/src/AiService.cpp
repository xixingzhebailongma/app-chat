#include "services/AiService.h"

#include <sstream>
#include <utility>

#include "services/AiTools.h"

namespace {
constexpr size_t kMaxToolResultChars = 4000;  // 单工具结果回喂上限，防上下文超限
}

AiService::AiService(std::shared_ptr<ILLMClient> llm,
                     std::shared_ptr<AiTools> tools, bool enableThinking,
                     int maxTokens, int maxToolIters)
    : llm_(std::move(llm)),
      tools_(std::move(tools)),
      enableThinking_(enableThinking),
      maxTokens_(maxTokens),
      maxToolIters_(maxToolIters) {}

std::string AiService::buildSystemPrompt(const std::string& role,
                                         const std::string& userId) const {
    std::ostringstream os;
    os << "你是智慧校园教室管理助手，帮助教师/管理员用自然语言查询与操作教室设备、"
          "场景、告警和进出记录。\n";
    os << "当前角色：" << role << "。\n";

    const auto spaces = tools_->accessibleSpaces(userId, role);
    os << "你可访问的空间：";
    if (spaces.empty()) {
        os << "无（尚未绑定教室）";
    } else {
        for (size_t i = 0; i < spaces.size(); ++i) {
            if (i) {
                os << "、";
            }
            os << spaces[i].space_id << "(" << spaces[i].name << ")";
        }
    }
    os << "\n\n规则：\n";
    os << "1. 只基于工具返回的真实数据回答，绝不编造数据。\n";
    os << "2. 涉及具体教室时，先调用 list_spaces 解析出 space_id，再调用对应工具。\n";
    os << "3. 工具调用可能因权限不足或安全策略被拒绝（例如门禁开门需二次确认）；如果工具返回被拒绝，请把原因如实转达给用户，不要谎称已执行。\n";
    os << "4. 用简洁中文回答，必要时列点。\n";
    return os.str();
}

nlohmann::json AiService::chat(const std::string& userId,
                               const std::string& role, const std::string& jwt,
                               const std::vector<nlohmann::json>& clientMessages) {
    const AiTools::Context ctx{userId, role, jwt};

    std::vector<nlohmann::json> messages;
    messages.push_back(
        {{"role", "system"}, {"content", buildSystemPrompt(role, userId)}});
    for (const auto& m : clientMessages) {
        // 只接受 user/assistant 文本（防御性：不接受客户端塞入 system/tool）。
        if (!m.is_object() || !m.contains("role") || !m["role"].is_string() ||
            !m.contains("content") || !m["content"].is_string()) {
            continue;
        }
        const std::string r = m["role"].get<std::string>();
        if (r != "user" && r != "assistant") {
            continue;
        }
        messages.push_back({{"role", r}, {"content", m["content"]}});
    }

    const auto tools = tools_->descriptors();
    nlohmann::json usedTools = nlohmann::json::array();

    for (int i = 0; i < maxToolIters_; ++i) {
        const auto r = llm_->chat(messages, tools, maxTokens_, enableThinking_);
        if (!r.ok) {
            return nlohmann::json{{"error", r.error.empty() ? "llm error" : r.error}};
        }
        if (r.finishReason == "stop") {
            return nlohmann::json{{"reply", r.content}, {"tool_calls", usedTools}};
        }
        if (r.finishReason == "tool_calls" && !r.toolCalls.empty()) {
            nlohmann::json assistantMsg = {{"role", "assistant"}, {"content", ""}};
            nlohmann::json tcs = nlohmann::json::array();
            for (const auto& tc : r.toolCalls) {
                tcs.push_back({{"id", tc.id},
                               {"type", "function"},
                               {"function",
                                {{"name", tc.name}, {"arguments", tc.arguments}}}});
            }
            assistantMsg["tool_calls"] = tcs;
            messages.push_back(assistantMsg);

            for (const auto& tc : r.toolCalls) {
                auto args = nlohmann::json::parse(tc.arguments, nullptr, false);
                if (args.is_discarded()) {
                    args = nlohmann::json::object();
                }
                const auto result = tools_->invoke(tc.name, args, ctx);
                std::string content = result.dump();
                if (content.size() > kMaxToolResultChars) {
                    content = content.substr(0, kMaxToolResultChars) + "…(截断)";
                }
                messages.push_back({{"role", "tool"},
                                    {"tool_call_id", tc.id},
                                    {"content", content}});
                usedTools.push_back(tc.name);
            }
            continue;
        }
        // finish_reason == length 或未知：内容被截断。
        return nlohmann::json{{"reply", r.content.empty() ? "回答被截断，请简化问题后重试。"
                                                          : r.content},
                              {"tool_calls", usedTools},
                              {"truncated", true}};
    }
    return nlohmann::json{{"reply", "工具调用次数过多，已停止。"},
                          {"tool_calls", usedTools}};
}
