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
                 bool enableDeviceControl)
    : deviceService_(std::move(deviceService)),
      deviceControl_(std::move(deviceControl)),
      sceneService_(std::move(sceneService)),
      alertService_(std::move(alertService)),
      accessRecordService_(std::move(accessRecordService)),
      spaceRepo_(std::move(spaceRepo)),
      userSpaces_(std::move(userSpaces)),
      enableDeviceControl_(enableDeviceControl) {}

std::vector<nlohmann::json> AiTools::descriptors() const {
    std::vector<nlohmann::json> tools = {
        fn("list_spaces", "列出当前用户可访问的教室/空间（space_id 与名称），用于把用户提到的教室名解析成 space_id。",
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

        fn("query_alerts", "查询告警列表。管理员可不传 space_id 查全局；教师自动限定到本人教室。status 可选 unhandled/handling/resolved。",
           nlohmann::json{{"type", "object"},
                          {"properties",
                           {{"space_id", {{"type", "string"}, {"description", "可选，教室 space_id"}}},
                            {"status", {{"type", "string"}, {"description", "可选，unhandled/handling/resolved"}}},
                            {"event_type", {{"type", "string"}, {"description", "可选，告警类型"}}},
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

        fn("execute_scene", "执行某教室的场景（如开启模式/离开模式/自定义场景）。",
           nlohmann::json{{"type", "object"},
                          {"properties",
                           {{"scene_id", {{"type", "string"}, {"description", "场景 scene_id"}}}}},
                          {"required", nlohmann::json::array({"scene_id"})}}),
    };
    if (enableDeviceControl_) {
        tools.push_back(
            fn("control_device", "开关单个设备（灯/综合屏/班牌等），仅管理员可用。command 只能 on/off/toggle。门禁类高风险操作仍需到小程序二次确认。",
               nlohmann::json{{"type", "object"},
                              {"properties",
                               {{"space_id", {{"type", "string"}, {"description", "教室 space_id"}}},
                                {"device_type", {{"type", "string"}, {"description", "设备类型，如 zigbee/fuhe-screen/fuhe-board"}}},
                                {"device_id", {{"type", "string"}, {"description", "设备 ID"}}},
                                {"command", {{"type", "string"}, {"enum", nlohmann::json::array({"on", "off", "toggle"})}}}}},
                              {"required", nlohmann::json::array({"space_id", "device_type", "device_id", "command"})}}));
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
                               const Context& ctx) const {
    if (name == "list_spaces") {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& s : accessibleSpaces(ctx.userId, ctx.role)) {
            arr.push_back(
                {{"space_id", s.space_id}, {"name", s.name}, {"type", s.type}});
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
        if (ctx.role != "admin") {
            return err("单设备控制仅管理员可用");
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

    return err("未知工具: " + name);
}
