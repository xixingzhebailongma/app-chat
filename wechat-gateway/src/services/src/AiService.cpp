#include "services/AiService.h"

#include <set>
#include <sstream>
#include <utility>
#include <vector>

#include <trantor/utils/Logger.h>

#include "services/AiConversationService.h"
#include "services/AiTools.h"

namespace {
constexpr size_t kMaxToolResultChars = 4000;  // 单工具结果回喂上限，防上下文超限
constexpr size_t kMaxContextMsgs = 20;        // 拼给模型的最近 N 条 user/assistant 历史
constexpr size_t kTitleMaxChars = 30;         // 会话标题 = 首条用户消息截断

// UTF-8 安全截断到 maxChars 个码点，避免截断在多字节字符中间。
std::string truncateUtf8(const std::string& s, size_t maxChars) {
    if (s.size() <= maxChars) {
        return s;
    }
    size_t i = 0;
    size_t n = 0;
    while (i < s.size() && n < maxChars) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        size_t len = 1;
        if ((c & 0x80) == 0) {
            len = 1;
        } else if ((c & 0xE0) == 0xC0) {
            len = 2;
        } else if ((c & 0xF0) == 0xE0) {
            len = 3;
        } else if ((c & 0xF8) == 0xF0) {
            len = 4;
        }
        if (i + len > s.size()) {
            break;
        }
        i += len;
        ++n;
    }
    return s.substr(0, i);
}

// 动作类工具：会真实改变设备/场景/绑定状态。回答若断言「已执行」，必须有
// 其中至少一个工具在本轮真正被调用作为证据，否则就是模型在编造。
const std::set<std::string>& kActionTools() {
    static const std::set<std::string> tools = {
        "control_device", "control_all_devices", "execute_scene",
        "bind_space", "unbind_space", "handle_alert"};
    return tools;
}

// 断言动作已发生的措辞。刻意不收「已查询/已列出」这类只读措辞，避免误伤
// 纯查询回复。只要最终回复出现这些词，就视为「声称执行了动作」。
// 注：故意不收「已解决」——它同时是告警状态字面量，纯查询回复里常出现
// （如「有 3 条已解决的告警」），收进去会误伤只读查询。
bool claimsAction(const std::string& reply) {
    static const std::vector<std::string> kws = {
        "已下发",  "下发成功", "已关闭", "已打开", "已开启", "已执行",
        "执行成功", "已绑定",   "绑定成功", "已解绑", "解绑成功", "已解除",
        "已处理",  "处理成功", "已标记",   "标记成功",
        "已重新",  "重新下发", "重新关闭", "重新打开", "重新执行"};
    for (const auto& kw : kws) {
        if (reply.find(kw) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool invokedActionTool(const nlohmann::json& usedTools) {
    for (const auto& name : usedTools) {
        if (kActionTools().count(name.get<std::string>())) {
            return true;
        }
    }
    return false;
}
}  // namespace

AiService::AiService(std::shared_ptr<ILLMClient> llm,
                     std::shared_ptr<AiTools> tools, bool enableThinking,
                     int maxTokens, int maxToolIters,
                     std::shared_ptr<AiConversationService> conversations)
    : llm_(std::move(llm)),
      tools_(std::move(tools)),
      conversations_(std::move(conversations)),
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
    os << "1. 只基于工具返回的真实数据回答；只有本轮真正调用了工具并读到返回，才能描述执行结果。绝不编造数据、设备清单或执行结果。\n";
    os << "2. 涉及具体教室时，先用 list_spaces 把教室名解析成 space_id，再调用对应工具。\n";
    os << "3. 工具调用被拒绝（权限不足/安全策略）时，把拒绝原因如实转达，不要谎称已执行。\n";
    os << "4. 用简洁中文回答，必要时列点。\n";
    os << "5. 「空间类型」是字典（list_space_types），与实际教室（list_spaces）是两回事；某类型还没有对应教室不代表用户没权限。\n";
    os << "6. 【二次确认范围】只有「门禁开门」需要二次确认（由网关返回 428 强制，不是你主动"
          "去反问用户）。其余——bind_space/unbind_space（绑定/解绑）、execute_scene（场景）、"
          "control_device/control_all_devices（设备开关）、handle_alert（标记告警）——都**不需要**"
          "二次确认，解析好参数后直接执行即可，不要反问「是否确认」。\n";
    os << "7. 【单设备 control_device】用户点名某一个设备开/关（照明/灯/综合屏/班牌/门禁）是单设备"
          "操作：**必须先 query_devices(space_id)** 拿到该设备的 device_id、device_type 和 status。"
          "若 status=offline（离线），如实说「该设备离线，无法控制」，**不要**调用 control_device，"
          "更不要声称已下发。若 online 才 control_device 下发 on/off，**不要**执行场景。管理员可控制"
          "全部教室，教师仅本人绑定教室。返回 ok/200 只表示「命令已下发」，用「已下发开/关指令」措辞，"
          "不要断言「已打开/关闭」。门禁开门需到小程序二次确认，收到 428/400 时如实说明。\n";
    os << "8. 【全部打开/关闭】用户说「把XX的所有设备/全部打开（关闭）」「全开/全关」时，**不是场景**，"
          "也不要提「开启/离开模式」：直接调用 control_all_devices(space_id, command)，command 用 on（打开）"
          "或 off（关闭）。该工具会自己跳过传感器和离线设备、逐台下发给剩余设备，并返回 summary/results——"
          "你必须按返回的 summary 逐台如实转述，禁止自行增删设备、改口，或声称「全部已打开/关闭」除非返回"
          "确实如此。\n";
    os << "9. 【场景/模式 execute_scene 尽量不碰】只要用户没有**明确说出**「模式」「场景」或某个具体"
          "场景名（如「开启模式」「离开模式」），就**一律不要**调用 execute_scene，也不要提任何模式/场景"
          "——一律按设备级操作处理（单设备 control_device / 全部设备逐台 control_device）。只有用户明确"
          "点名场景时才 execute_scene。执行后，返回的 results 数组是**实际**匹配并下发的设备清单（含 "
          "label/device_id/command/status）：你必须按 results 逐台如实报告——某台设备不在 results 里，"
          "就说明它没被匹配/没下发，必须明确说「XX 未关闭/未打开」。"
          "applied 为 0 或 warning 非空时，如实说明没有设备被控制。场景不含门禁，执行后说明门禁需到小程序"
          "单独操作。\n";
    os << "10. 【绑定/解绑 bind_space/unbind_space】仅管理员。**硬性要求：任何一次"
          " bind_space/unbind_space 调用前，都必须先调用 list_teachers 把教师姓名解析成真实"
          " user_id、list_spaces 把教室名解析成真实 space_id；user_id/space_id 必须取自这两个"
          "工具返回的结果，禁止使用示例值、猜测或编造 ID。** 解析通过后直接调用执行，**不要反问"
          "「是否确认」**。只有返回 ok:true 才能说「已绑定/已解除」，否则如实转达失败原因。若"
          " list_teachers/list_spaces 返回里解析不到该姓名/教室，必须停止操作并向用户说明找不到"
          "该教师/教室，禁止猜测 ID 继续。\n";
    os << "11. 用户用姓名（如「李老师」）提到教师时，先用 list_teachers 解析成 user_id，不要把"
          "姓名当 user_id 传。\n";
    os << "12. 【设备在线/离线只能查、不能猜】设备的「在线/离线」状态**只能**来自 query_devices 返回"
          "的 status 字段，绝不凭空编造。不知道当前状态就先 query_devices 查；被用户质疑状态时也重新"
          "query_devices 查最新值再据实回答，禁止凭空改口（一会儿在线、一会儿离线）。\n";
    os << "13. 【被质疑时绝不编造】用户质疑「为什么没关/没执行/没生效」时：不要凭空道歉、不要编造"
          "「已重新下发/已重新关闭」、也不要为上一轮编原因。先重新调用 query_devices 核实最新状态"
          "再据实回答；若上一轮你并未真正调用动作工具执行，就如实承认「上一轮未执行」并询问是否现在"
          "执行。\n";
    os << "14. 【标记告警 handle_alert】仅管理员。**硬性要求：alert_id 必须先调用 query_alerts "
          "解析得到，禁止猜测或编造。** 用户说「已处理/标记处理/处理掉」对应 status=resolved，"
          "「处理中」对应 status=handling。只有工具返回不含 error（含 alert_id/status）才能说"
          "「已处理/已解决」，否则如实转达失败原因；被拒绝（非管理员）时如实说「需要管理员权限」。\n";
    os << "15. 【告警默认查全局，别自作主张限定教室】用户提到「告警/处理/标记」但**没有明确说哪个"
          "教室**时，query_alerts **一律不传 space_id**（管理员查全局，教师自动限定本人教室），"
          "禁止默认成某个教室（如 A101）。查完后：匹配的告警**只有一条**，就直接用它的 alert_id "
          "调 handle_alert，不要反问；**多条**（不同教室或不同设备）才把候选（教室+类型+设备）列出来，"
          "让用户指定要处理哪条，再 handle_alert。用户明确点名教室时才传 space_id。\n";
    return os.str();
}

nlohmann::json AiService::chat(const std::string& userId,
                               const std::string& role, const std::string& jwt,
                               const std::vector<nlohmann::json>& clientMessages,
                               const std::string& conversationId) {
    AiTools::Context ctx{userId, role, jwt};
    // 1. 过滤本轮新增消息：只接受 user/assistant 文本（防御性：不接受客户端
    //    塞入 system/tool）。
    std::vector<nlohmann::json> turnMessages;
    std::vector<AiMessage> turnAiMessages;
    std::string title;
    for (const auto& m : clientMessages) {
        if (!m.is_object() || !m.contains("role") || !m["role"].is_string() ||
            !m.contains("content") || !m["content"].is_string()) {
            continue;
        }
        const std::string r = m["role"].get<std::string>();
        if (r != "user" && r != "assistant") {
            continue;
        }
        const std::string content = m["content"].get<std::string>();
        turnMessages.push_back({{"role", r}, {"content", content}});
        turnAiMessages.push_back(AiMessage{r, content, ""});
        if (title.empty() && r == "user") {
            title = truncateUtf8(content, kTitleMaxChars);
        }
    }
    if (turnMessages.empty()) {
        return nlohmann::json{{"error", "no valid messages"}};
    }

    // 2. 解析会话。conversations_ 为空 => 无状态（旧行为），不落库、不回传会话 id。
    std::string convId = conversationId;
    bool isNew = false;
    if (conversations_) {
        if (convId.empty()) {
            convId = AiConversationService::newConversationId();
            isNew = true;
        } else {
            const auto conv = conversations_->get(userId, convId);
            if (!conv) {
                return nlohmann::json{{"error", "conversation not found"},
                                      {"status", 404}};
            }
        }
    }

    // 3. 拼上下文：system + 最近 20 条历史 + 本轮。
    std::vector<nlohmann::json> messages;
    messages.push_back(
        {{"role", "system"}, {"content", buildSystemPrompt(role, userId)}});
    if (conversations_ && !convId.empty()) {
        const auto history = conversations_->messages(convId, kMaxContextMsgs);
        for (const auto& h : history) {
            messages.push_back({{"role", h.role}, {"content", h.content}});
        }
    }
    for (const auto& m : turnMessages) {
        messages.push_back(m);
    }

    // 4. 工具调用循环（与旧实现一致）。
    const auto tools = tools_->descriptors();
    nlohmann::json usedTools = nlohmann::json::array();

    std::string reply;
    bool truncated = false;
    bool finished = false;
    for (int i = 0; i < maxToolIters_; ++i) {
        const auto r = llm_->chat(messages, tools, maxTokens_, enableThinking_);
        LOG_INFO << "[ai] user=" << userId << " role=" << role << " iter=" << i
                 << " finish=" << r.finishReason
                 << " n_tool_calls=" << r.toolCalls.size();
        if (!r.ok) {
            return nlohmann::json{{"error",
                                   r.error.empty() ? "llm error" : r.error}};
        }
        if (r.finishReason == "stop") {
            if (usedTools.empty()) {
                LOG_WARN << "[ai] user=" << userId
                         << " answered without any tool call (possible fabrication)";
            }
            reply = r.content;
            finished = true;
            break;
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
        reply = r.content.empty() ? "回答被截断，请简化问题后重试。" : r.content;
        truncated = true;
        finished = true;
        break;
    }
    if (!finished) {
        reply = "工具调用次数过多，已停止。";
    }

    // 4.5 确定性护栏：杜绝「没执行却谎称已执行」。小模型在思考关闭或上下文
    // 较长时，会把「已下发/已关闭/已重新…」直接编出来而不调用任何动作工具。
    // 一旦发现最终回复断言了动作、但本轮没有任何动作工具调用佐证，就覆盖为诚实
    // 说明——否则这句假话既直接骗用户，又会在下面落库写进会话历史，下一轮被
    // 质疑时模型会顺着自己的谎言继续编（「抱歉已重新下发」），形成滚雪球。
    if (!reply.empty() && claimsAction(reply) && !invokedActionTool(usedTools)) {
        LOG_WARN << "[ai] user=" << userId
                 << " action claim without action tool; reply rewritten";
        reply = "抱歉，我本轮并没有实际执行任何设备控制、场景或绑定操作，"
                "无法确认这些设备的状态。请把需求再说一遍，我会先查询当前设备"
                "状态、确认后再执行。";
    }

    // 5. 整轮落库：user 消息 + assistant 回复（原子；失败轮不落库，避免孤儿会话）。
    if (conversations_ && !convId.empty()) {
        AiConversation conv;
        conv.conversation_id = convId;
        conv.user_id = userId;
        conv.title = title;
        std::vector<AiMessage> toSave = turnAiMessages;
        toSave.push_back(AiMessage{"assistant", reply, ""});
        conversations_->saveTurn(conv, isNew, toSave);
    }

    nlohmann::json out = {{"reply", reply},
                          {"tool_calls", usedTools},
                          {"conversation_id", convId}};
    if (truncated) {
        out["truncated"] = true;
    }
    return out;
}
