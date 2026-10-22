// AI 助手服务单测：假 LLM（ILLMClient 实现）+ 内存仓库，覆盖函数调用循环、
// 坏 JSON 参数、未知工具、LLM 不可达、迭代上限，以及 control_device 收口
// （默认不注册 + 仅 admin）。无真实网络 / Qwen / Postgres / go-backend。
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
#include "db/UserSpacesRepository.h"
#include "services/AccessRecordService.h"
#include "services/AiService.h"
#include "services/AiTools.h"
#include "services/AlertService.h"
#include "services/SceneService.h"

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
                                     userSpaces, enableDeviceControl);
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

    std::cout << "\n=== " << (failures == 0 ? "ALL PASS"
                                           : "FAILURES: " + std::to_string(failures))
              << " ===\n";
    return failures == 0 ? 0 : 1;
}
