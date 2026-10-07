#pragma once

#include <nlohmann/json.hpp>
#include <string>

// POST /api/miniapp/device/control 的请求 DTO（设计文档 八 控制指令转发）。
struct DeviceControlRequest {
    std::string space_id;     // 必填；依据 user_spaces 校验教师权限
    std::string device_type;  // 必填；作为路径段转发
    std::string device_id;    // 必填；作为路径段转发
    std::string command;      // 必填；白名单由服务端强制校验
    bool confirm = false;     // 可选；高风险指令必填

    static bool fromJson(const nlohmann::json& j,
                         DeviceControlRequest& out,
                         std::string& err) {
        if (!j.is_object()) {
            err = "body must be a JSON object";
            return false;
        }

        auto requireString = [&](const char* key, std::string& dst) {
            if (!j.contains(key) || !j[key].is_string() ||
                j[key].get<std::string>().empty()) {
                err = std::string(key) + " is required";
                return false;
            }
            dst = j[key].get<std::string>();
            return true;
        };

        if (!requireString("space_id", out.space_id) ||
            !requireString("device_type", out.device_type) ||
            !requireString("device_id", out.device_id) ||
            !requireString("command", out.command)) {
            return false;
        }

        // 路径段安全：device_type / device_id 会作为 URL 路径段
        // 转发到 go-backend，因此拒绝可能跳出目标路径的字符
        // （斜杠、反斜杠、查询、片段、百分号）以及超长值。
        auto validPathSegment = [&](const char* key, const std::string& v) {
            if (v.find_first_of("/\\? #%") != std::string::npos) {
                err = std::string(key) + " contains invalid characters";
                return false;
            }
            if (v.size() > 128) {
                err = std::string(key) + " is too long";
                return false;
            }
            return true;
        };

        if (!validPathSegment("device_type", out.device_type) ||
            !validPathSegment("device_id", out.device_id)) {
            return false;
        }

        if (j.contains("confirm")) {
            if (!j["confirm"].is_boolean()) {
                err = "confirm must be a boolean";
                return false;
            }
            out.confirm = j["confirm"].get<bool>();
        }

        return true;
    }
};
