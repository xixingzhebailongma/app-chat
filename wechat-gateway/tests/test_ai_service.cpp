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
#include "db/AlertRepository.h"
#include "db/SceneRepository.h"
#include "db/SpaceRepository.h"
#include "db/SpaceTypeRepository.h"
#include "db/UserRoleRepository.h"
#include "db/UserSpacesRepository.h"
#include "db/WechatBindingRepository.h"
#include "services/AccessRecordService.h"
#include "services/AiService.h"
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

    LLMChatResult chat(const std::vector<nlohmann::json>&,
                       const std::vector<nlohmann::json>&, int,
                       bool) const override {
        if (idx < script.size()) {
            return script[idx++];
        }
        LLMChatResult r;
        r.ok = true;
        r.finishReason = "stop";
        r.content = "(script exhausted)";
        return r;
    }
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
// 首期开关默认关时不会触达；测试 admin-only 时在鉴权处提前返回，也不会触达。
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
                  r["error"].get<std::string>().find("仅管理员") !=
                      std::string::npos,
              "收口：control_device 非 admin 被拒");
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

        auto rt = tools->invoke("list_teachers", nlohmann::json::object(), admin);
        check(rt.contains("teachers") && rt["teachers"].is_array() &&
                  rt["teachers"].size() == 1 &&
                  rt["teachers"][0]["user_id"] == "u_teacher_1",
              "list_teachers 返回教师清单含 user_id（供姓名解析）");

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
        check(r1.contains("error") &&
                  r1["error"].get<std::string>().find("二次确认") !=
                      std::string::npos,
              "bind_space 不带 confirmed 返回二次确认");

        auto r2 = tools->invoke("bind_space",
                                nlohmann::json{{"user_id", "u_teacher_1"},
                                               {"space_id", "spc_a"},
                                               {"confirmed", true}},
                                admin);
        check(r2.contains("ok") && r2["ok"] == true,
              "bind_space 确认后绑定成功");
        const auto sids = userSpaces->spacesForUser("u_teacher_1");
        check(std::find(sids.begin(), sids.end(), "spc_a") != sids.end(),
              "bind_space 确认后 user_spaces 已写入");
    }

    std::cout << "\n=== " << (failures == 0 ? "ALL PASS"
                                           : "FAILURES: " + std::to_string(failures))
              << " ===\n";
    return failures == 0 ? 0 : 1;
}
