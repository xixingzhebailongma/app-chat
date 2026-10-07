#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

// POST /internal/notify/send 的 DTO —— 依据设计文档 7.1 节。

// 【已废弃】通知目标声明。收件人实际由 PushRule（target_roles / to_space_teacher）
// 派生（NotifyService::resolveRecipients），请求里的 target 不再决定派发，
// 仅作为 notify_logs 的审计回放字段保留，生产端可省略。
struct NotifyTarget {
    std::vector<std::string> roles;  // 例如 ["admin"] / ["admin","teacher"]
    bool space_teachers = false;

    nlohmann::json toJson() const {
        return nlohmann::json{{"roles", roles}, {"space_teachers", space_teachers}};
    }

    static bool fromJson(const nlohmann::json& j,
                         NotifyTarget& out,
                         std::string& err) {
        if (!j.is_object()) {
            err = "target must be an object";
            return false;
        }
        if (j.contains("roles")) {
            if (!j["roles"].is_array()) {
                err = "target.roles must be an array";
                return false;
            }
            for (const auto& r : j["roles"]) {
                if (!r.is_string()) {
                    err = "target.roles must be an array of strings";
                    return false;
                }
                out.roles.push_back(r.get<std::string>());
            }
        }
        if (out.roles.empty()) {
            err = "target.roles is required and must be non-empty";
            return false;
        }
        if (j.contains("space_teachers")) {
            if (!j["space_teachers"].is_boolean()) {
                err = "target.space_teachers must be a boolean";
                return false;
            }
            out.space_teachers = j["space_teachers"].get<bool>();
        }
        return true;
    }
};

struct NotifySendRequest {
    std::string event_id;      // 可选
    std::string event_type;    // 必填
    std::string space_id;      // 可选
    std::string device_id;     // 可选（用于内部冷却键）
    std::string device_label;  // 可选
    std::string severity;      // 可选，默认 "info"
    std::string content;       // 必填
    NotifyTarget target;       // 已废弃（可选）：仅审计回放，收件人由 PushRule 派生

    // 可选的 7.6 事件属性，供 PushRule 条件作为
    // 防御性复核使用（设计文档 九 推送分级）。哨兵值表示
    // 该属性缺失——生产者（go-backend）才是 7.6 过滤的
    // 权威，因此缺失的属性绝不会抑制事件。
    std::string transition;      // 例如 "online->offline"
    int duration_sec = -1;       // 传感器超阈值时长
    int consecutive = -1;        // 人脸登录连续失败次数
    int offline_minutes = -1;    // 外设离线分钟数

    nlohmann::json toJson() const {
        nlohmann::json j{{"event_type", event_type},
                         {"content", content}};
        if (!target.roles.empty()) j["target"] = target.toJson();
        if (!event_id.empty()) j["event_id"] = event_id;
        if (!space_id.empty()) j["space_id"] = space_id;
        if (!device_id.empty()) j["device_id"] = device_id;
        if (!device_label.empty()) j["device_label"] = device_label;
        if (!severity.empty()) j["severity"] = severity;
        if (!transition.empty()) j["transition"] = transition;
        if (duration_sec >= 0) j["duration_sec"] = duration_sec;
        if (consecutive >= 0) j["consecutive"] = consecutive;
        if (offline_minutes >= 0) j["offline_minutes"] = offline_minutes;
        return j;
    }

    static bool fromJson(const nlohmann::json& j,
                         NotifySendRequest& out,
                         std::string& err) {
        if (!j.is_object()) {
            err = "body must be a JSON object";
            return false;
        }

        if (!j.contains("event_type") || !j["event_type"].is_string() ||
            j["event_type"].get<std::string>().empty()) {
            err = "event_type is required";
            return false;
        }
        out.event_type = j["event_type"].get<std::string>();

        if (!j.contains("content") || !j["content"].is_string() ||
            j["content"].get<std::string>().empty()) {
            err = "content is required";
            return false;
        }
        out.content = j["content"].get<std::string>();

        // target 已废弃：收件人由 PushRule 派生，此字段仅用于 notify_logs
        // 审计回放。生产端可省略；若提供则尽力解析，解析失败不阻塞（保留为空）。
        if (j.contains("target") && j["target"].is_object()) {
            std::string ignored;
            NotifyTarget::fromJson(j["target"], out.target, ignored);
        }

        if (j.contains("event_id") && j["event_id"].is_string())
            out.event_id = j["event_id"].get<std::string>();
        if (j.contains("space_id") && j["space_id"].is_string())
            out.space_id = j["space_id"].get<std::string>();
        if (j.contains("device_id") && j["device_id"].is_string())
            out.device_id = j["device_id"].get<std::string>();
        if (j.contains("device_label") && j["device_label"].is_string())
            out.device_label = j["device_label"].get<std::string>();
        if (j.contains("severity") && j["severity"].is_string())
            out.severity = j["severity"].get<std::string>();
        if (out.severity.empty())
            out.severity = "info";

        if (j.contains("transition") && j["transition"].is_string())
            out.transition = j["transition"].get<std::string>();
        if (j.contains("duration_sec") && j["duration_sec"].is_number_integer())
            out.duration_sec = j["duration_sec"].get<int>();
        if (j.contains("consecutive") && j["consecutive"].is_number_integer())
            out.consecutive = j["consecutive"].get<int>();
        if (j.contains("offline_minutes") &&
            j["offline_minutes"].is_number_integer())
            out.offline_minutes = j["offline_minutes"].get<int>();

        return true;
    }
};

struct NotifySendResponse {
    bool accepted = false;
    std::string notify_id;
    std::vector<std::string> channels_tried;
    std::string final_status;

    nlohmann::json toJson() const {
        return nlohmann::json{{"accepted", accepted},
                              {"notify_id", notify_id},
                              {"channels_tried", channels_tried},
                              {"final_status", final_status}};
    }
};
