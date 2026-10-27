// AI 助手服务单测：假 LLM（ILLMClient 实现）+ 内存仓库，覆盖函数调用循环、
// 坏 JSON 参数、未知工具、LLM 不可达、迭代上限，以及 control_device 收口
// （默认不注册 + 仅 admin）。无真实网络 / Qwen / Postgres / go-backend。
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "clients/ILLMClient.h"
#include "db/AccessRecordRepository.h"
#include "db/AiConversationRepository.h"
#include "db/AlertRepository.h"
#include "db/SceneRepository.h"
#include "db/SpaceRepository.h"
#include "db/SpaceTypeRepository.h"
#include "db/UserRoleRepository.h"
#include "db/UserSpacesRepository.h"
#include "db/WechatBindingRepository.h"
#include "services/AccessRecordService.h"
#include "services/AiService.h"
#include "services/AiConversationService.h"
#include "services/AiTools.h"
#include "services/AlertService.h"
#include "services/SceneService.h"
#include "services/SpaceBindingService.h"

namespace {

int failures = 0;

void check(bool ok, const std::string& label) {
    if (ok) {
        std::cout << "PASS  " << label << "\n";
    } else {
        std::cout << "FAIL  " << label << "\n";
        ++failures;
    }
}

nlohmann::json userMsg(const std::string& content) {
    nlohmann::json j = nlohmann::json::object();
    j["role"] = "user";
    j["content"] = content;
    return j;
}

// 假 LLM：按脚本依次返回，用尽后回 stop（不触网）。
class FakeLlm : public ILLMClient {
public:
    std::vector<LLMChatResult> script;
    mutable size_t idx = 0;
    mutable std::vector<std::vector<nlohmann::json>> captured;

    LLMChatResult chat(const std::vector<nlohmann::json>& msgs,
                       const std::vector<nlohmann::json>&, int,
                       bool) const override {
        captured.push_back(msgs);
        if (idx < script.size()) {
            return script[idx++];
        }
        LLMChatResult r;
        r.ok = true;
        r.finishReason = "stop";
        r.content = "(script exhausted)";
        return r;
    }

    void reset() { idx = 0; }
};

LLMChatResult toolCall(const std::string& name, const std::string& args) {
    LLMChatResult r;
    r.ok = true;
    r.finishReason = "tool_calls";
    r.toolCalls.push_back({"call_1", name, args});
    return r;
}

LLMChatResult stopResult(const std::string& content) {
    LLMChatResult r;
    r.ok = true;
    r.finishReason = "stop";
    r.content = content;
    return r;
}

LLMChatResult errorResult(const std::string& err) {
    LLMChatResult r;
    r.ok = false;
    r.error = err;
    return r;
}

// 构建真实 AiTools（内存仓库 + 同步服务）。device/deviceControl 传 nullptr——
// 首期开关默认关时不会触达；开关开时 control_device 会先命中 deviceControl 空指针
// 保护返回「控制服务不可用」，同样不会触达真实网络。
std::shared_ptr<AiTools> makeTools(bool enableDeviceControl) {
    auto userSpaces = std::make_shared<InMemoryUserSpacesRepository>();
    userSpaces->add("u_teacher_1", "spc_a");

    auto spaceRepo = std::make_shared<InMemorySpaceRepository>();
    spaceRepo->add(Space{"spc_a", "A101 教室", "standard"});
    spaceRepo->add(Space{"spc_b", "B202 教室", "standard"});

    auto spaceTypes = std::make_shared<InMemorySpaceTypeRepository>();
    spaceTypes->add(SpaceType{"standard", "标准教室", 0, true, ""});
    spaceTypes->add(SpaceType{"lecture", "多媒体报告厅", 1, true, ""});
    spaceTypes->add(SpaceType{"gym", "体育馆", 2, true, ""});

    auto sceneRepo = std::make_shared<InMemorySceneRepository>();
    auto alertRepo = std::make_shared<InMemoryAlertRepository>();
    auto accessRepo = std::make_shared<InMemoryAccessRecordRepository>();

    auto sceneService = std::make_shared<SceneService>(
        nullptr, userSpaces, spaceRepo, sceneRepo, nullptr, "");
    auto alertService =
        std::make_shared<AlertService>(alertRepo, userSpaces, nullptr, "");
    auto accessService =
        std::make_shared<AccessRecordService>(accessRepo, userSpaces);

    return std::make_shared<AiTools>(nullptr, nullptr, sceneService,
                                     alertService, accessService, spaceRepo,
                                     userSpaces, spaceTypes, nullptr,
                                     enableDeviceControl);
}

}  // namespace

int main() {
    {
        auto fake = std::make_shared<FakeLlm>();
        AiService svc(fake, makeTools(false), /*enableThinking=*/false,
                      /*maxTokens=*/1024, /*maxToolIters=*/3);

        fake->script = {toolCall("list_spaces", "{}"),
                        stopResult("你好，A101 可用")};
        auto r = svc.chat("u_admin_1", "admin", "jwt", {userMsg("hi")});
        check(r.value("reply", "") == "你好，A101 可用", "工具循环：返回最终回复");
        check(r["tool_calls"].size() == 1 &&
                  r["tool_calls"][0] == "list_spaces",
              "工具循环：记录工具调用");
    }

    {
        auto fake = std::make_shared<FakeLlm>();
        AiService svc(fake, makeTools(false), false, 1024, 3);
        fake->script = {toolCall("list_scenes", "{not valid json"),
                        stopResult("done")};
        auto r = svc.chat("u_admin_1", "admin", "jwt", {userMsg("x")});
        check(r.value("reply", "") == "done", "坏 JSON 参数：不崩溃且循环收敛");
    }

    {
        auto fake = std::make_shared<FakeLlm>();
        AiService svc(fake, makeTools(false), false, 1024, 3);
        fake->script = {toolCall("nonexistent", "{}"), stopResult("done")};
        auto r = svc.chat("u_admin_1", "admin", "jwt", {userMsg("x")});
        check(r.value("reply", "") == "done", "未知工具：不崩溃且循环收敛");
    }

    {
        auto fake = std::make_shared<FakeLlm>();
        AiService svc(fake, makeTools(false), false, 1024, 3);
        fake->script = {errorResult("llm unreachable")};
        auto r = svc.chat("u_admin_1", "admin", "jwt", {userMsg("x")});
        check(r.contains("error") && r["error"] == "llm unreachable",
              "LLM 不可达：返回 error 不崩溃");
    }

    {
        auto fake = std::make_shared<FakeLlm>();
        AiService svc(fake, makeTools(false), false, 1024, 3);
        fake->script = {toolCall("list_spaces", "{}"), toolCall("list_spaces", "{}"),
                        toolCall("list_spaces", "{}"), toolCall("list_spaces", "{}")};
        auto r = svc.chat("u_admin_1", "admin", "jwt", {userMsg("x")});
        check(r.value("reply", "").find("工具调用次数过多") != std::string::npos,
              "迭代上限：第 3 轮后停止");
    }

    {
        // 护栏：没有任何动作工具调用却谎称「已下发」→ 覆盖为诚实说明，
        // 不把假话回给用户 / 不写进历史（杜绝下一轮滚雪球）。
        auto fake = std::make_shared<FakeLlm>();
        AiService svc(fake, makeTools(false), false, 1024, 3);
        fake->script = {stopResult(
            "已下发关闭A101教室电子班牌和教室照明的指令")};
        auto r = svc.chat("u_admin_1", "admin", "jwt",
                          {userMsg("把A101的电子班牌和照明关了")});
        check(r.value("reply", "").find("并没有实际执行") != std::string::npos &&
                  r.value("reply", "").find("关闭A101") == std::string::npos,
              "护栏：谎称已下发且无动作工具 → 覆盖为诚实说明");
    }

    {
        // 护栏：被质疑时编造「已重新关闭」→ 同样覆盖（覆盖发生在落库之前）。
        auto fake = std::make_shared<FakeLlm>();
        AiService svc(fake, makeTools(false), false, 1024, 3);
        fake->script = {stopResult(
            "抱歉，刚才的指令可能未成功下发。已重新为您关闭A101教室的电子班牌和教室照明。")};
        auto r = svc.chat("u_admin_1", "admin", "jwt",
                          {userMsg("为什么刚才的指令没有下发？")});
        check(r.value("reply", "").find("并没有实际执行") != std::string::npos,
              "护栏：编造「已重新关闭」且无动作工具 → 覆盖为诚实说明");
    }

    {
        // 护栏不误伤：本轮确有动作工具调用佐证时，措辞即便含「已下发」也放行
        // （交由模型据工具结果转述）。
        auto fake = std::make_shared<FakeLlm>();
        AiService svc(fake, makeTools(false), false, 1024, 3);
        fake->script = {toolCall("execute_scene", "{\"scene_id\":\"scn_none\"}"),
                        stopResult("已下发关闭指令")};
        auto r = svc.chat("u_admin_1", "admin", "jwt",
                          {userMsg("执行关闭场景")});
        check(r.value("reply", "") == "已下发关闭指令",
              "护栏不误伤：有动作工具调用时放行原回复");
    }

    {
        auto tools = makeTools(false);
        bool hasControl = false;
        for (const auto& d : tools->descriptors()) {
            if (d["function"]["name"] == "control_device") {
                hasControl = true;
            }
        }
        check(!hasControl, "收口：control_device 默认不在工具列表");

        AiTools::Context admin{"u_admin_1", "admin", "jwt"};
        auto r = tools->invoke("control_device", nlohmann::json::object(), admin);
        check(r.contains("error") &&
                  r["error"].get<std::string>().find("未知工具") !=
                      std::string::npos,
              "收口：开关关时 invoke control_device 返回未知工具");
    }

    {
        auto tools = makeTools(true);
        AiTools::Context teacher{"u_teacher_1", "teacher", "jwt"};
        auto r = tools->invoke(
            "control_device",
            nlohmann::json{{"space_id", "spc_a"},
                           {"device_type", "zigbee"},
                           {"device_id", "d1"},
                           {"command", "on"}},
            teacher);
        check(r.contains("error") &&
                  r["error"].get<std::string>().find("控制服务不可用") !=
                      std::string::npos,
              "control_device 教师放行到下层控制服务（本测试无 deviceControl）");
    }

    {
        auto tools = makeTools(false);
        AiTools::Context admin{"u_admin_1", "admin", "jwt"};
        auto r = tools->invoke("list_space_types", nlohmann::json::object(),
                               admin);
        check(r.is_array() && r.size() == 3,
              "list_space_types 返回全部字典类型");
        bool hasGym = false;
        for (const auto& t : r) {
            if (t.value("code", "") == "gym" &&
                t.value("name", "") == "体育馆") {
                hasGym = true;
            }
        }
        check(hasGym, "list_space_types 含 gym(体育馆)");
    }

    {
        // bind_space/unbind_space：注册 + 仅管理员 + 二次确认门槛 + 确认后真正绑定。
        auto userSpaces = std::make_shared<InMemoryUserSpacesRepository>();
        auto userRoles = std::make_shared<InMemoryUserRoleRepository>();
        userRoles->setRoles("u_teacher_1", {"teacher"});
        auto spaceRepo = std::make_shared<InMemorySpaceRepository>();
        spaceRepo->add(Space{"spc_a", "A101 教室", "standard"});
        auto bindings = std::make_shared<InMemoryWechatBindingRepository>();
        auto spaceTypes = std::make_shared<InMemorySpaceTypeRepository>();
        auto spaceBinding = std::make_shared<SpaceBindingService>(
            userSpaces, userRoles, spaceRepo, bindings, nullptr, "");

        auto tools = std::make_shared<AiTools>(
            nullptr, nullptr, nullptr, nullptr, nullptr, spaceRepo, userSpaces,
            spaceTypes, spaceBinding, /*enableDeviceControl=*/false);

        bool hasBind = false, hasUnbind = false, hasTeachers = false;
        for (const auto& d : tools->descriptors()) {
            const std::string n = d["function"]["name"];
            if (n == "bind_space") {
                hasBind = true;
            }
            if (n == "unbind_space") {
                hasUnbind = true;
            }
            if (n == "list_teachers") {
                hasTeachers = true;
            }
        }
        check(hasBind && hasUnbind, "注册：bind_space/unbind_space 在工具列表");
        check(hasTeachers, "注册：list_teachers 在工具列表");

        AiTools::Context admin{"u_admin_1", "admin", "jwt"};
        AiTools::Context teacher{"u_teacher_1", "teacher", "jwt"};

        // 硬约束：未先 list_teachers/list_spaces 解析就直接 unbind → 拒绝。
        auto rPre = tools->invoke(
            "unbind_space",
            nlohmann::json{{"user_id", "u_teacher_1"},
                           {"space_id", "spc_a"}},
            admin);
        check(rPre.contains("error") &&
                  rPre["error"].get<std::string>().find("解析") !=
                      std::string::npos,
              "硬约束：未解析直接 unbind 被拒");

        auto rt = tools->invoke("list_teachers", nlohmann::json::object(), admin);
        check(rt.contains("teachers") && rt["teachers"].is_array() &&
                  rt["teachers"].size() == 1 &&
                  rt["teachers"][0]["user_id"] == "u_teacher_1",
              "list_teachers 返回教师清单含 user_id（供姓名解析）");

        // 解析空间，填充 admin.resolvedSpaceIds（bind/unbind 硬约束需要）。
        tools->invoke("list_spaces", nlohmann::json::object(), admin);

        auto r0 = tools->invoke("bind_space",
                                nlohmann::json{{"user_id", "u_teacher_1"},
                                               {"space_id", "spc_a"}},
                                teacher);
        check(r0.contains("error") &&
                  r0["error"].get<std::string>().find("仅管理员") !=
                      std::string::npos,
              "bind_space 非 admin 被拒");

        auto r1 = tools->invoke("bind_space",
                                nlohmann::json{{"user_id", "u_teacher_1"},
                                               {"space_id", "spc_a"}},
                                admin);
        check(r1.contains("ok") && r1["ok"] == true,
              "bind_space 直接绑定成功（无需二次确认）");
        const auto sids = userSpaces->spacesForUser("u_teacher_1");
        check(std::find(sids.begin(), sids.end(), "spc_a") != sids.end(),
              "bind_space 直接绑定后 user_spaces 已写入");

        // unbind_space：未解析的 user_id（如 AI 编造 teacher_001）被硬约束拒绝。
        auto ru = tools->invoke("unbind_space",
                                nlohmann::json{{"user_id", "teacher_001"},
                                               {"space_id", "spc_a"}},
                                admin);
        check(ru.dump().find("解析") != std::string::npos,
              "unbind_space 未解析的 user_id 被拒（防 AI 编造 user_id）");

        // unbind_space：合法教师 + 解析通过后直接解绑（无需二次确认）。
        auto rv = tools->invoke("unbind_space",
                                nlohmann::json{{"user_id", "u_teacher_1"},
                                               {"space_id", "spc_a"}},
                                admin);
        check(rv.contains("ok") && rv["ok"] == true,
              "unbind_space 直接解绑成功");
        const auto afterUnbind = userSpaces->spacesForUser("u_teacher_1");
        check(std::find(afterUnbind.begin(), afterUnbind.end(), "spc_a") ==
                  afterUnbind.end(),
              "unbind_space 直接解绑后 user_spaces 已移除");
    }

    {
        // handle_alert：注册 + 仅管理员 + alert_id 硬约束（必须先 query_alerts
        // 解析）+ 成功写入。防「标错告警」或「编造 alert_id」。
        auto userSpaces = std::make_shared<InMemoryUserSpacesRepository>();
        auto userRoles = std::make_shared<InMemoryUserRoleRepository>();
        userRoles->setRoles("u_teacher_1", {"teacher"});
        auto spaceRepo = std::make_shared<InMemorySpaceRepository>();
        spaceRepo->add(Space{"spc_a", "A101 教室", "standard"});
        auto alertRepo = std::make_shared<InMemoryAlertRepository>();
        Alert a;
        a.alert_id = "alt_1";
        a.space_id = "spc_a";
        a.event_type = "device_offline";
        a.status = "unhandled";
        a.device_id = "dev_fuhe_a101";
        a.device_label = "A101 综合屏";
        alertRepo->add(a);
        auto alertService = std::make_shared<AlertService>(
            alertRepo, userSpaces, nullptr, "");
        auto spaceTypes = std::make_shared<InMemorySpaceTypeRepository>();
        auto bindings = std::make_shared<InMemoryWechatBindingRepository>();
        auto spaceBinding = std::make_shared<SpaceBindingService>(
            userSpaces, userRoles, spaceRepo, bindings, nullptr, "");
        auto tools = std::make_shared<AiTools>(
            nullptr, nullptr, nullptr, alertService, nullptr, spaceRepo,
            userSpaces, spaceTypes, spaceBinding, /*enableDeviceControl=*/false);

        bool hasHandle = false;
        for (const auto& d : tools->descriptors()) {
            if (d["function"]["name"] == "handle_alert") {
                hasHandle = true;
            }
        }
        check(hasHandle, "注册：handle_alert 在工具列表");

        AiTools::Context admin{"u_admin_1", "admin", "jwt"};
        AiTools::Context teacher{"u_teacher_1", "teacher", "jwt"};

        // 硬约束：未先 query_alerts 解析就直接 handle → 拒绝。
        auto r0 = tools->invoke("handle_alert",
                                nlohmann::json{{"alert_id", "alt_1"},
                                               {"status", "resolved"}},
                                admin);
        check(r0.contains("error") &&
                  r0["error"].get<std::string>().find("query_alerts") !=
                      std::string::npos,
              "handle_alert 未解析 alert_id 被拒（防 AI 编造 id）");

        // 非管理员被拒。
        auto rTeacher = tools->invoke("handle_alert",
                                      nlohmann::json{{"alert_id", "alt_1"},
                                                     {"status", "resolved"}},
                                      teacher);
        check(rTeacher.contains("error") &&
                  rTeacher["error"].get<std::string>().find("仅管理员") !=
                      std::string::npos,
              "handle_alert 非 admin 被拒");

        // 先 query_alerts 解析 alert_id，再 handle → 成功。
        tools->invoke("query_alerts", nlohmann::json::object(), admin);
        auto rOk = tools->invoke("handle_alert",
                                 nlohmann::json{{"alert_id", "alt_1"},
                                                {"status", "resolved"}},
                                 admin);
        check(rOk.contains("status") && rOk["status"] == "resolved",
              "handle_alert 解析后成功标记为 resolved");
        check(alertRepo->findById("alt_1")->status == "resolved",
              "handle_alert 成功写入告警状态");

        // 非法 status 被拒。
        auto rBad = tools->invoke("handle_alert",
                                  nlohmann::json{{"alert_id", "alt_1"},
                                                 {"status", "deleted"}},
                                  admin);
        check(rBad.contains("error"),
              "handle_alert 非法 status 被拒");
    }

    {
        // 回归（端到端）：解绑必须「先解析 → 直接执行」，且不照抄示例值。
        auto userSpaces = std::make_shared<InMemoryUserSpacesRepository>();
        userSpaces->add("u_teacher_1", "spc_a");
        auto userRoles = std::make_shared<InMemoryUserRoleRepository>();
        userRoles->setRoles("u_teacher_1", {"teacher"});
        auto spaceRepo = std::make_shared<InMemorySpaceRepository>();
        spaceRepo->add(Space{"spc_a", "A101 教室", "standard"});
        auto bindings = std::make_shared<InMemoryWechatBindingRepository>();
        auto spaceTypes = std::make_shared<InMemorySpaceTypeRepository>();
        auto spaceBinding = std::make_shared<SpaceBindingService>(
            userSpaces, userRoles, spaceRepo, bindings, nullptr, "");
        auto tools = std::make_shared<AiTools>(
            nullptr, nullptr, nullptr, nullptr, nullptr, spaceRepo, userSpaces,
            spaceTypes, spaceBinding, /*enableDeviceControl=*/false);
        auto fake = std::make_shared<FakeLlm>();
        AiService svc(fake, tools, false, 1024, 6);

        // 第 1 轮：list_teachers → list_spaces → unbind_space → 直接完成。
        fake->script = {
            toolCall("list_teachers", "{}"),
            toolCall("list_spaces", "{}"),
            toolCall("unbind_space",
                     "{\"user_id\":\"u_teacher_1\",\"space_id\":\"spc_a\"}"),
            stopResult("已解除 李老师 与 A101 的绑定"),
        };
        auto r = svc.chat("u_admin_1", "admin", "jwt",
                          {userMsg("取消李老师与 A101 的绑定")}, "");

        // system prompt 不再含 dev 种子真实值，用工具名占位指引。
        check(!fake->captured.empty(), "回归：捕获到 system prompt");
        const std::string sys = fake->captured[0][0].value("content", "");
        check(sys.find("u_teacher_1") == std::string::npos &&
                  sys.find("spc_a8acdd5c") == std::string::npos,
              "回归：system prompt 不含 dev 种子示例值");
        check(sys.find("list_teachers") != std::string::npos &&
                  sys.find("list_spaces") != std::string::npos,
              "回归：system prompt 用 list_teachers/list_spaces 占位指引");

        // 工具调用顺序：先 list_teachers/list_spaces 解析，再 unbind_space。
        check(r["tool_calls"].size() == 3 &&
                  r["tool_calls"][0] == "list_teachers" &&
                  r["tool_calls"][1] == "list_spaces" &&
                  r["tool_calls"][2] == "unbind_space",
              "回归：第 1 轮先 list_teachers/list_spaces 再 unbind_space");
        // 第 1 轮直接执行完成（回复含「已解除」，不要求二次确认）。
        check(r.value("reply", "").find("已解除") != std::string::npos,
              "回归：解绑第 1 轮直接执行（无二次确认）");
    }

    {
        // 会话记忆：conversation_id 生成 / 跨轮上下文 / 只落 user+assistant。
        auto repo = std::make_shared<InMemoryAiConversationRepository>();
        auto convSvc = std::make_shared<AiConversationService>(repo);
        auto fake = std::make_shared<FakeLlm>();
        AiService svc(fake, makeTools(false), false, 1024, 3, convSvc);

        fake->script = {stopResult("第一轮回答")};
        auto r1 = svc.chat("u_admin_1", "admin", "jwt", {userMsg("你好")}, "");
        const std::string convId = r1.value("conversation_id", "");
        check(!convId.empty(), "记忆：无 id 新建会话并返回 conversation_id");
        check(r1.value("reply", "") == "第一轮回答", "记忆：第一轮回复");

        fake->reset();
        fake->script = {stopResult("第二轮回答")};
        auto r2 = svc.chat("u_admin_1", "admin", "jwt",
                           {userMsg("第二个问题")}, convId);
        check(r2.value("conversation_id", "") == convId, "记忆：沿用同一会话");

        bool sawFirstUser = false, sawFirstReply = false;
        for (const auto& call : fake->captured) {
            for (const auto& m : call) {
                if (m.value("role", "") == "user" &&
                    m.value("content", "") == "你好") {
                    sawFirstUser = true;
                }
                if (m.value("role", "") == "assistant" &&
                    m.value("content", "") == "第一轮回答") {
                    sawFirstReply = true;
                }
            }
        }
        check(sawFirstUser && sawFirstReply, "记忆：第二轮上下文含第一轮一问一答");

        const auto msgs = repo->listMessages(convId, -1);
        check(msgs.size() == 4, "记忆：落库 4 条（2 问 2 答）");
        bool anyTool = false;
        for (const auto& m : msgs) {
            if (m.role != "user" && m.role != "assistant") {
                anyTool = true;
            }
        }
        check(!anyTool, "记忆：落库 role 仅 user/assistant（无工具消息）");
    }

    {
        // 归属隔离：非本人 get 为 nullopt，续聊他人会话返回 404。
        auto repo = std::make_shared<InMemoryAiConversationRepository>();
        auto convSvc = std::make_shared<AiConversationService>(repo);
        auto fake = std::make_shared<FakeLlm>();
        AiService svc(fake, makeTools(false), false, 1024, 3, convSvc);
        fake->script = {stopResult("ok")};
        auto r = svc.chat("u_admin_1", "admin", "jwt", {userMsg("hi")}, "");
        const std::string convId = r.value("conversation_id", "");

        check(!convSvc->get("u_teacher_1", convId).has_value(),
              "记忆：非本人 get 返回 nullopt");
        auto r2 = svc.chat("u_teacher_1", "teacher", "jwt", {userMsg("x")}, convId);
        check(r2.contains("error") && r2.value("status", 0) == 404,
              "记忆：续聊他人会话返回 404");
    }

    {
        // conversations 为空：退化为无状态，行为同接入记忆前。
        auto fake = std::make_shared<FakeLlm>();
        AiService svc(fake, makeTools(false), false, 1024, 3);
        fake->script = {stopResult("legacy")};
        auto r = svc.chat("u_admin_1", "admin", "jwt", {userMsg("hi")}, "");
        check(r.value("reply", "") == "legacy" &&
                  r.value("conversation_id", "") == "",
              "记忆：conversations 为空退化为无状态（无 conversation_id）");
    }

    std::cout << "\n=== " << (failures == 0 ? "ALL PASS"
                                           : "FAILURES: " + std::to_string(failures))
              << " ===\n";
    return failures == 0 ? 0 : 1;
}
