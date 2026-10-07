#pragma once

#include <nlohmann/json.hpp>
#include <string>

// POST /api/miniapp/scene/execute 的请求 DTO（设计文档 7.4② 场景快捷控制）。
struct SceneExecuteRequest {
    std::string space_id;  // 必填；依据 user_spaces 校验权限
    std::string scene_id;  // 必填；服务端按 SceneDefinitions 展开

    static bool fromJson(const nlohmann::json& j,
                         SceneExecuteRequest& out,
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
            !requireString("scene_id", out.scene_id)) {
            return false;
        }
        return true;
    }
};
