#include "services/AiService.h"

#include <sstream>
#include <utility>

#include <trantor/utils/Logger.h>

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
    os << "5. 「空间类型」是字典（可用 list_space_types 查询 code+中文名），与实际存在的教室（list_spaces）是两回事；某类型还没有对应教室不代表用户没有权限。\n";
    os << "6. 单设备开关用 control_device（仅管理员可用）；门禁开门需到小程序二次确认，收到 428/400 时如实说明。control_device 返回成功（ok/200）只表示「命令已下发、被接收」，不等于设备已实际执行——请用「已下发关闭指令」这类措辞，不要断言「已成功关闭/打开」。\n";
    os << "7. 「把所有设备关了/开了」没有一键工具。正确做法：先 list_scenes(space_id) "
          "找到内置「离开模式」（关灯+关屏+关班牌）或「开启模式」（开灯+唤醒综合屏+开班牌）"
          "并 execute_scene；执行后如实说明门禁不在场景范围内、需用户到小程序单独操作。"
          "仅在用户明确要逐台操作时，才 query_devices 后逐个 control_device（仅管理员可用）。\n";
    os << "8. 只有在你本轮真正调用了工具并读到工具返回后，才能描述执行结果；整轮没有调用"
          "任何工具时，禁止声称「已下发/已关闭/已打开，包括…」或编造设备清单。"
          "做不到（门禁需二次确认、无权限、无对应工具）就说明原因，绝不编结果。\n";
    os << "9. 绑定/解绑教师-教室（bind_space/unbind_space）仅管理员可用且是敏感操作："
          "先 list_spaces 把教室名解析成 space_id、确认目标 user_id 是教师，并在真正执行前"
          "向用户说明并征得明确同意；未征得同意不得自行传 confirmed=true。首次调用"
          "（不带 confirmed）返回需二次确认时，如实转达给用户，不要自行补 confirmed。\n";
    os << "10. 用户用姓名（如「李老师」）提到教师时，先用 list_teachers 把姓名解析成 user_id，"
          "再调用 bind_space/unbind_space；不要把姓名直接当 user_id 传。\n";
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
        LOG_INFO << "[ai] user=" << userId << " role=" << role << " iter=" << i
                 << " finish=" << r.finishReason
                 << " n_tool_calls=" << r.toolCalls.size();
        if (!r.ok) {
            return nlohmann::json{{"error", r.error.empty() ? "llm error" : r.error}};
        }
        if (r.finishReason == "stop") {
            if (usedTools.empty()) {
                LOG_WARN << "[ai] user=" << userId
                         << " answered without any tool call (possible fabrication)";
            }
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
                LOG_INFO << "[ai] tool=" << tc.name << " args=" << tc.arguments;
                auto args = nlohmann::json::parse(tc.arguments, nullptr, false);
                if (args.is_discarded()) {
                    args = nlohmann::json::object();
                }
                const auto result = tools_->invoke(tc.name, args, ctx);
                std::string content = result.dump();
                if (content.size() > kMaxToolResultChars) {
                    content = content.substr(0, kMaxToolResultChars) + "…(截断)";
                }
                LOG_INFO << "[ai] tool_result=" << tc.name
                         << " bytes=" << content.size();
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
