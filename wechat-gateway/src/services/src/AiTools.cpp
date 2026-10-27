#include "services/AiTools.h"

#include <chrono>
#include <future>
#include <memory>
#include <optional>
#include <utility>

#include <drogon/HttpTypes.h>

#include "services/AccessRecordService.h"
#include "services/AlertService.h"
#include "services/DeviceControlService.h"
#include "services/DeviceService.h"
#include "services/SceneService.h"
#include "services/SpaceBindingService.h"
#include "utils/Authz.h"

namespace {

// 构造一个 OpenAI function-calling 工具描述符。
nlohmann::json fn(const std::string& name, const std::string& desc,
                  nlohmann::json params) {
    return nlohmann::json{{"type", "function"},
                          {"function", {{"name", name},
                                        {"description", desc},
                                        {"parameters", std::move(params)}}}};
}

// 从 args 取字符串参数；缺失/非字符串返回空串。
std::string argString(const nlohmann::json& args, const char* key) {
    if (!args.is_object() || !args.contains(key) || !args[key].is_string()) {
        return "";
    }
    return args[key].get<std::string>();
}

int argInt(const nlohmann::json& args, const char* key, int def) {
    if (!args.is_object() || !args.contains(key) ||
        !args[key].is_number_integer()) {
        return def;
    }
    return args[key].get<int>();
}

nlohmann::json err(const std::string& msg) {
    return nlohmann::json{{"error", msg}};
}

// 上游异步服务（回调式）→ 同步等待桥。用 shared_ptr<promise> 避免超时后
// 回调再写已析构 promise 的悬垂问题；超时返回 nullopt。
template <typename T, typename F>
std::optional<T> wait(F&& start) {
    auto pp = std::make_shared<std::promise<T>>();
    auto fut = pp->get_future();
    start([pp](const T& r) { pp->set_value(r); });
    if (fut.wait_for(std::chrono::seconds(15)) != std::future_status::ready) {
        return std::nullopt;
    }
    return fut.get();
}

// 把服务返回的 body 字符串解析成 JSON；失败返回 {"error": ...}。
nlohmann::json parseBody(const std::string& body) {
    auto j = nlohmann::json::parse(body, nullptr, false);
    if (j.is_discarded()) {
        return err("上游返回非 JSON");
    }
    return j;
}

}  // namespace

AiTools::AiTools(std::shared_ptr<DeviceService> deviceService,
                 std::shared_ptr<DeviceControlService> deviceControl,
                 std::shared_ptr<SceneService> sceneService,
                 std::shared_ptr<AlertService> alertService,
                 std::shared_ptr<AccessRecordService> accessRecordService,
                 std::shared_ptr<SpaceRepository> spaceRepo,
                 std::shared_ptr<UserSpacesRepository> userSpaces,
                 std::shared_ptr<SpaceTypeRepository> spaceTypes,
                 std::shared_ptr<SpaceBindingService> spaceBinding,
                 bool enableDeviceControl)
    : deviceService_(std::move(deviceService)),
      deviceControl_(std::move(deviceControl)),
      sceneService_(std::move(sceneService)),
      alertService_(std::move(alertService)),
      accessRecordService_(std::move(accessRecordService)),
      spaceRepo_(std::move(spaceRepo)),
      userSpaces_(std::move(userSpaces)),
      spaceTypes_(std::move(spaceTypes)),
      spaceBinding_(std::move(spaceBinding)),
      enableDeviceControl_(enableDeviceControl) {}

std::vector<nlohmann::json> AiTools::descriptors() const {
    std::vector<nlohmann::json> tools = {
        fn("list_spaces", "列出当前用户可访问的教室/空间（space_id 与名称），用于把用户提到的教室名解析成 space_id。",
           nlohmann::json{{"type", "object"}, {"properties", nlohmann::json::object()},
                          {"required", nlohmann::json::array()}}),

        fn("list_space_types", "列出系统定义的所有空间类型字典（code 与中文名）。这是类型字典，与实际存在的教室（list_spaces）是两回事；某类型可能还没有对应教室。",
           nlohmann::json{{"type", "object"}, {"properties", nlohmann::json::object()},
                          {"required", nlohmann::json::array()}}),

        fn("query_devices", "查询某教室的设备列表与在线/离线状态（综合屏、灯、传感器、编解码器等）。",
           nlohmann::json{{"type", "object"},
                          {"properties",
                           {{"space_id", {{"type", "string"}, {"description", "教室 space_id"}}}}},
                          {"required", nlohmann::json::array({"space_id"})}}),

        fn("list_scenes", "列出某教室的场景（开启模式/离开模式/自定义场景）。",
           nlohmann::json{{"type", "object"},
                          {"properties",
                           {{"space_id", {{"type", "string"}, {"description", "教室 space_id"}}}}},
                          {"required", nlohmann::json::array({"space_id"})}}),

        fn("query_alerts", "查询告警列表。用户没有明确说哪个教室时**不要传 space_id**（管理员查全局；教师自动限定本人教室），只有用户明确点名教室才传 space_id。status 可选 unhandled/handling/resolved；event_type 可选（如 device_offline）。",
           nlohmann::json{{"type", "object"},
                          {"properties",
                           {{"space_id", {{"type", "string"}, {"description", "可选，教室 space_id；用户没提教室就留空查全局"}}},
                            {"status", {{"type", "string"}, {"description", "可选，unhandled/handling/resolved"}}},
                            {"event_type", {{"type", "string"}, {"description", "可选，告警类型，如 device_offline"}}},
                            {"page", {{"type", "integer"}, {"description", "可选，页码，从 1 起"}}}}},
                          {"required", nlohmann::json::array()}}),

        fn("query_access_records", "查询某教室的进出记录（人脸/刷卡登录）。date 可选，格式 YYYY-MM-DD。",
           nlohmann::json{{"type", "object"},
                          {"properties",
                           {{"space_id", {{"type", "string"}, {"description", "教室 space_id"}}},
                            {"date", {{"type", "string"}, {"description", "可选，YYYY-MM-DD"}}}}},
                          {"required", nlohmann::json::array({"space_id"})}}),

        fn("alert_stats", "统计告警（按状态/类型/教室聚合计数），用于概览。",
           nlohmann::json{{"type", "object"}, {"properties", nlohmann::json::object()},
                          {"required", nlohmann::json::array()}}),

        fn("handle_alert",
           "标记某条告警的处理状态（仅管理员可用）。alert_id 必须先用 query_alerts 解析得到。status：handling=处理中 / resolved=已解决。用户说「已处理/标记处理/处理掉」对应 resolved；「处理中」对应 handling。remark 可选。返回含 error 即失败，须如实转达；否则（含 alert_id/status）即成功，才能说「已处理」。",
           nlohmann::json{{"type", "object"},
                          {"properties",
                           {{"alert_id", {{"type", "string"}, {"description", "告警 alert_id，必须来自 query_alerts 返回"}}},
                            {"status", {{"type", "string"}, {"enum", nlohmann::json::array({"handling", "resolved"})}}},
                            {"remark", {{"type", "string"}, {"description", "可选，处理备注"}}}}},
                          {"required", nlohmann::json::array({"alert_id", "status"})}}),

        fn("execute_scene",
           "执行某教室的场景。内置「开启模式」=开灯+唤醒综合屏+开班牌，「离开模式」=关灯+关屏+关班牌（均不含门禁）。**只有用户明确说出「模式/场景」或某个具体场景名时才用本工具**；「全部打开/关闭」和单设备开关都用 control_device，不要用本工具。返回的 applied 是实际下发设备数、warning 非空表示有设备未匹配（如未匹配到可控制设备），需按实际 applied/warning 如实转述，不得声称未下发的设备已执行。",
           nlohmann::json{{"type", "object"},
                          {"properties",
                           {{"scene_id", {{"type", "string"}, {"description", "场景 scene_id"}}}}},
                          {"required", nlohmann::json::array({"scene_id"})}}),

        fn("list_teachers",
           "列出所有教师及其 user_id、姓名、当前绑定的教室（space_id）。当用户用姓名（如「李老师」）提到某教师时，先用本工具把姓名解析成 user_id，再调用 bind_space/unbind_space。",
           nlohmann::json{{"type", "object"}, {"properties", nlohmann::json::object()},
                          {"required", nlohmann::json::array()}}),

        fn("bind_space",
           "给教师绑定一个教室（仅管理员可用）。参数：user_id（教师 user_id，必须用 list_teachers 解析姓名得到）、space_id（教室，必须用 list_spaces 解析得到）。解析通过后直接执行，无需二次确认。只有返回 ok:true 才能说「已绑定」，并据返回的 space_ids 如实报告该教师当前绑定。",
           nlohmann::json{{"type", "object"},
                          {"properties",
                           {{"user_id", {{"type", "string"}, {"description", "教师 user_id，来自 list_teachers"}}},
                            {"space_id", {{"type", "string"}, {"description", "教室 space_id，来自 list_spaces"}}}}},
                          {"required", nlohmann::json::array({"user_id", "space_id"})}}),

        fn("unbind_space",
           "解绑教师的某个教室（仅管理员可用）。参数：user_id（教师 user_id，必须用 list_teachers 解析姓名得到）、space_id（教室，必须用 list_spaces 解析得到）。解析通过后直接执行，无需二次确认。只有返回 ok:true 才能说「已解除」，并据返回的 space_ids 如实报告该教师当前绑定；若 space_ids 仍含目标教室，说明解绑未生效，须如实告知用户。",
           nlohmann::json{{"type", "object"},
                          {"properties",
                           {{"user_id", {{"type", "string"}, {"description", "教师 user_id，来自 list_teachers"}}},
                            {"space_id", {{"type", "string"}, {"description", "教室 space_id，来自 list_spaces"}}}}},
                          {"required", nlohmann::json::array({"user_id", "space_id"})}}),
    };
    if (enableDeviceControl_) {
        tools.push_back(
            fn("control_device", "开关单个设备（灯/综合屏/班牌等）。管理员可控制全部教室，教师仅本人绑定教室（越权返回 403）。command 只能 on/off/toggle。门禁类高风险操作仍需到小程序二次确认。",
               nlohmann::json{{"type", "object"},
                              {"properties",
                               {{"space_id", {{"type", "string"}, {"description", "教室 space_id"}}},
                                {"device_type", {{"type", "string"}, {"description", "设备类型，如 zigbee/fuhe-screen/fuhe-board"}}},
                                {"device_id", {{"type", "string"}, {"description", "设备 ID"}}},
                                {"command", {{"type", "string"}, {"enum", nlohmann::json::array({"on", "off", "toggle"})}}}}},
                              {"required", nlohmann::json::array({"space_id", "device_type", "device_id", "command"})}}));

        tools.push_back(
            fn("control_all_devices",
               "打开/关闭某个教室的**所有**可控设备（用户说「全部打开/关闭」「把所有设备开了/关了」时用本工具，不要用 execute_scene）。参数：space_id、command（on=打开 / off=关闭）。本工具会自动跳过传感器和离线设备，逐台下发给剩余设备，并返回逐台结果与一段可直接转述的 summary；你必须按返回的 summary/results 如实转述，禁止自行增删设备或改口。",
               nlohmann::json{{"type", "object"},
                              {"properties",
                               {{"space_id", {{"type", "string"}, {"description", "教室 space_id"}}},
                                {"command", {{"type", "string"}, {"enum", nlohmann::json::array({"on", "off"})}}}}},
                              {"required", nlohmann::json::array({"space_id", "command"})}}));
    }
    return tools;
}

std::vector<Space> AiTools::accessibleSpaces(const std::string& userId,
                                             const std::string& role) const {
    std::vector<Space> out;
    if (!spaceRepo_ || !userSpaces_) {
        return out;
    }
    for (const auto& s : spaceRepo_->listEnabled()) {
        if (authz::canAccessSpace(role, userId, s.space_id, *userSpaces_)) {
            out.push_back(s);
        }
    }
    return out;
}

nlohmann::json AiTools::invoke(const std::string& name,
                               const nlohmann::json& args,
                               Context& ctx) const {
    if (name == "list_spaces") {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& s : accessibleSpaces(ctx.userId, ctx.role)) {
            arr.push_back(
                {{"space_id", s.space_id}, {"name", s.name}, {"type", s.type}});
            ctx.resolvedSpaceIds.insert(s.space_id);
        }
        return arr;
    }

    if (name == "list_space_types") {
        if (!spaceTypes_) {
            return err("空间类型字典不可用");
        }
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& t : spaceTypes_->listAll()) {
            arr.push_back(
                {{"code", t.code}, {"name", t.name}, {"enabled", t.enabled}});
        }
        return arr;
    }

    if (name == "query_devices") {
        const std::string spaceId = argString(args, "space_id");
        if (spaceId.empty()) {
            return err("缺少 space_id，请先调用 list_spaces");
        }
        const auto maybe = wait<DeviceResult>([&](auto cb) {
            deviceService_->listDevices(ctx.userId, ctx.role, ctx.jwt, spaceId,
                                        cb);
        });
        if (!maybe) {
            return err("查询设备超时");
        }
        if (maybe->status != drogon::k200OK) {
            return err("http " + std::to_string(static_cast<int>(maybe->status)));
        }
        return parseBody(maybe->body);
    }

    if (name == "list_scenes") {
        const std::string spaceId = argString(args, "space_id");
        if (spaceId.empty()) {
            return err("缺少 space_id，请先调用 list_spaces");
        }
        const auto r = sceneService_->list(ctx.userId, ctx.role, spaceId);
        if (r.status != drogon::k200OK) {
            return err("http " + std::to_string(static_cast<int>(r.status)));
        }
        return r.body;
    }

    if (name == "query_alerts") {
        AlertFilter filter;
        filter.event_type = argString(args, "event_type");
        filter.status = argString(args, "status");
        const std::string spaceId = argString(args, "space_id");
        const int page = argInt(args, "page", 1);
        const auto r =
            alertService_->list(ctx.userId, ctx.role, spaceId, filter, page, 20);
        if (r.status != drogon::k200OK) {
            return err("http " + std::to_string(static_cast<int>(r.status)));
        }
        // 记录本页返回的 alert_id，作为 handle_alert 的硬约束（禁止猜/编 id）。
        if (r.body.is_object() && r.body.contains("alerts") &&
            r.body["alerts"].is_array()) {
            for (const auto& a : r.body["alerts"]) {
                if (a.is_object() && a.contains("alert_id") &&
                    a["alert_id"].is_string()) {
                    ctx.resolvedAlertIds.insert(a["alert_id"].get<std::string>());
                }
            }
        }
        return r.body;
    }

    if (name == "query_access_records") {
        const std::string spaceId = argString(args, "space_id");
        if (spaceId.empty()) {
            return err("缺少 space_id，请先调用 list_spaces");
        }
        const std::string date = argString(args, "date");
        const auto r = accessRecordService_->list(ctx.userId, ctx.role, spaceId,
                                                  date, 1, 20, "");
        if (r.status != drogon::k200OK) {
            return err("http " + std::to_string(static_cast<int>(r.status)));
        }
        return r.body;
    }

    if (name == "alert_stats") {
        const auto r = alertService_->stats(ctx.userId, ctx.role);
        if (r.status != drogon::k200OK) {
            return err("http " + std::to_string(static_cast<int>(r.status)));
        }
        return r.body;
    }

    if (name == "handle_alert") {
        if (ctx.role != "admin") {
            return err("标记告警仅管理员可用");
        }
        if (!alertService_) {
            return err("告警服务不可用");
        }
        const std::string alertId = argString(args, "alert_id");
        const std::string status = argString(args, "status");
        const std::string remark = argString(args, "remark");
        if (alertId.empty() || status.empty()) {
            return err("参数不全：需要 alert_id/status");
        }
        if (status != "handling" && status != "resolved") {
            return err("status 只能是 handling/resolved");
        }
        // 硬约束：alert_id 必须来自 query_alerts 解析，禁止猜测（防止标错真实告警）。
        if (ctx.resolvedAlertIds.count(alertId) == 0) {
            return err("alert_id 未通过 query_alerts 解析，禁止猜测。请先调用 "
                       "query_alerts 拿到真实 alert_id 后再标记。");
        }
        const auto r =
            alertService_->handle(alertId, ctx.userId, ctx.role, remark, status);
        return r.body;  // 成功 {"alert_id","status"}；错误信封原样透传
    }

    if (name == "execute_scene") {
        const std::string sceneId = argString(args, "scene_id");
        if (sceneId.empty()) {
            return err("缺少 scene_id，请先调用 list_scenes");
        }
        const auto maybe = wait<SceneResult>([&](auto cb) {
            sceneService_->execute(ctx.userId, ctx.role, ctx.jwt, sceneId, cb);
        });
        if (!maybe) {
            return err("执行场景超时");
        }
        if (maybe->status != drogon::k200OK) {
            return err("http " + std::to_string(static_cast<int>(maybe->status)));
        }
        return parseBody(maybe->body);
    }

    if (name == "control_device") {
        // 首期收口：默认不注册（descriptors 不含），此处再兜底——开关未开时视为未知工具。
        if (!enableDeviceControl_) {
            return err("未知工具: control_device");
        }
        // 鉴权下沉到 DeviceControlService::control（authz::canAccessSpace）：
        // admin 全量、teacher 仅本人绑定空间、其余 403。本层不再做角色门槛。
        if (!deviceControl_) {
            return err("控制服务不可用");
        }
        const std::string spaceId = argString(args, "space_id");
        const std::string deviceType = argString(args, "device_type");
        const std::string deviceId = argString(args, "device_id");
        const std::string command = argString(args, "command");
        if (spaceId.empty() || deviceType.empty() || deviceId.empty() ||
            command.empty()) {
            return err("参数不全：需要 space_id/device_type/device_id/command");
        }
        if (command != "on" && command != "off" && command != "toggle") {
            return err("command 只能是 on/off/toggle");
        }
        // MVP：不绕过门禁二次确认——confirm 恒 false；门禁 off/toggle 命中现有
        // 428 CONFIRM_REQUIRED / 400，原样回传，由模型转述「需到小程序操作」。
        const auto maybe = wait<DeviceControlResult>([&](auto cb) {
            deviceControl_->control(ctx.userId, ctx.role, ctx.jwt, spaceId,
                                    deviceType, deviceId, command, false, cb);
        });
        if (!maybe) {
            return err("控制设备超时");
        }
        // 成功回显或错误信封（门禁 off 未确认 428 / toggle 400 / 越权 403）都原样
        // 透传，让模型读到具体原因并如实转述给用户。
        return parseBody(maybe->body);
    }

    if (name == "control_all_devices") {
        if (!enableDeviceControl_) {
            return err("未知工具: control_all_devices");
        }
        if (!deviceService_ || !deviceControl_) {
            return err("控制服务不可用");
        }
        const std::string spaceId = argString(args, "space_id");
        const std::string command = argString(args, "command");
        if (spaceId.empty() || (command != "on" && command != "off")) {
            return err("参数不全：需要 space_id 和 command(on/off)");
        }

        // 1. 查设备列表（网关侧空间鉴权在 DeviceService 内完成）。
        const auto q = wait<DeviceResult>([&](auto cb) {
            deviceService_->listDevices(ctx.userId, ctx.role, ctx.jwt, spaceId,
                                        cb);
        });
        if (!q) {
            return err("查询设备超时");
        }
        if (q->status != drogon::k200OK) {
            return err("http " + std::to_string(static_cast<int>(q->status)));
        }
        auto parsed = nlohmann::json::parse(q->body, nullptr, false);
        if (parsed.is_discarded()) {
            return err("上游返回非 JSON");
        }
        nlohmann::json devs;
        if (parsed.is_array()) {
            devs = parsed;
        } else if (parsed.is_object() && parsed.contains("devices") &&
                   parsed["devices"].is_array()) {
            devs = parsed["devices"];
        } else {
            return err("设备列表格式异常");
        }

        // 2. 逐台处理：跳过传感器/离线设备，其余逐台下发。
        const std::string verb = (command == "on") ? "开" : "关";
        nlohmann::json results = nlohmann::json::array();
        nlohmann::json skipped = nlohmann::json::array();
        std::vector<std::string> parts;
        for (const auto& d : devs) {
            if (!d.is_object()) {
                continue;
            }
            const std::string id = d.value("device_id", "");
            const std::string label = d.value("label", "");
            const std::string status = d.value("status", "");
            const std::string zbType = d.value("zb_type", "");
            const std::string devType = d.value("device_type", "");
            if (id.empty()) {
                continue;
            }
            if (zbType == "sensor") {
                skipped.push_back({{"device_id", id},
                                   {"label", label},
                                   {"reason", "sensor"}});
                parts.push_back(label + "（传感器，跳过）");
                continue;
            }
            if (status == "offline") {
                skipped.push_back({{"device_id", id},
                                   {"label", label},
                                   {"reason", "offline"}});
                parts.push_back(label + "（离线，跳过）");
                continue;
            }
            const auto cr = wait<DeviceControlResult>([&](auto cb) {
                deviceControl_->control(ctx.userId, ctx.role, ctx.jwt, spaceId,
                                        devType, id, command, false, cb);
            });
            if (!cr) {
                results.push_back({{"device_id", id},
                                   {"label", label},
                                   {"command", command},
                                   {"ok", false},
                                   {"note", "超时"}});
                parts.push_back(label + "（控制超时）");
                continue;
            }
            const int st = static_cast<int>(cr->status);
            const bool ok = st >= 200 && st < 300;
            results.push_back({{"device_id", id},
                               {"label", label},
                               {"command", command},
                               {"status", st},
                               {"ok", ok}});
            if (st == 428) {
                parts.push_back(label + "（需到小程序二次确认）");
            } else if (ok) {
                parts.push_back(label + "：已下发" + verb);
            } else {
                parts.push_back(label + "：下发失败");
            }
        }

        std::string summary;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i) {
                summary += "；";
            }
            summary += parts[i];
        }
        return nlohmann::json{{"ok", true},
                              {"space_id", spaceId},
                              {"command", command},
                              {"summary", summary},
                              {"results", results},
                              {"skipped", skipped}};
    }

    if (name == "list_teachers") {
        if (ctx.role != "admin") {
            return err("列出教师仅管理员可用");
        }
        if (!spaceBinding_) {
            return err("绑定服务不可用");
        }
        const auto r = spaceBinding_->list(ctx.role);
        if (r.body.contains("teachers") && r.body["teachers"].is_array()) {
            for (const auto& t : r.body["teachers"]) {
                if (t.is_object() && t.contains("user_id") &&
                    t["user_id"].is_string()) {
                    ctx.resolvedTeacherIds.insert(t["user_id"].get<std::string>());
                }
            }
        }
        return r.body;  // {"teachers":[{user_id,name,space_ids}]}
    }

    if (name == "bind_space") {
        if (ctx.role != "admin") {
            return err("绑定教师教室仅管理员可用");
        }
        const std::string teacherUserId = argString(args, "user_id");
        const std::string spaceId = argString(args, "space_id");
        if (teacherUserId.empty() || spaceId.empty()) {
            return err("参数不全：需要 user_id/space_id");
        }
        // 硬约束：user_id/space_id 必须来自 list_teachers/list_spaces 的解析结果，
        // 禁止猜测或使用示例值。解析通过后直接执行（不再二次确认）。
        if (ctx.resolvedTeacherIds.count(teacherUserId) == 0 ||
            ctx.resolvedSpaceIds.count(spaceId) == 0) {
            return err("user_id/space_id 未通过 list_teachers/list_spaces 解析，"
                       "禁止猜测或使用示例值。请先调用 list_teachers 解析教师、"
                       "list_spaces 解析教室，用返回的真实 ID 再试。");
        }
        if (!spaceBinding_) {
            return err("绑定服务不可用");
        }
        const auto r =
            spaceBinding_->bind(ctx.role, ctx.userId, teacherUserId, spaceId);
        return r.body;  // 成功 {"ok":true} 或错误信封原样透传
    }

    if (name == "unbind_space") {
        if (ctx.role != "admin") {
            return err("解绑教师教室仅管理员可用");
        }
        const std::string teacherUserId = argString(args, "user_id");
        const std::string spaceId = argString(args, "space_id");
        if (teacherUserId.empty() || spaceId.empty()) {
            return err("参数不全：需要 user_id/space_id");
        }
        // 硬约束：user_id/space_id 必须来自 list_teachers/list_spaces 的解析结果，
        // 禁止猜测或使用示例值。解析通过后直接执行（不再二次确认）。
        if (ctx.resolvedTeacherIds.count(teacherUserId) == 0 ||
            ctx.resolvedSpaceIds.count(spaceId) == 0) {
            return err("user_id/space_id 未通过 list_teachers/list_spaces 解析，"
                       "禁止猜测或使用示例值。请先调用 list_teachers 解析教师、"
                       "list_spaces 解析教室，用返回的真实 ID 再试。");
        }
        if (!spaceBinding_) {
            return err("绑定服务不可用");
        }
        const auto r =
            spaceBinding_->unbind(ctx.role, ctx.userId, teacherUserId, spaceId);
        return r.body;  // 成功 {"ok":true} 或错误信封原样透传
    }

    return err("未知工具: " + name);
}
