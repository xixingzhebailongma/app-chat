#pragma once

#include <nlohmann/json.hpp>
#include <string>

// POST /internal/oa/arrival-notify 的 DTO —— 依据设计文档 7.1 节。

struct ArrivalNotifyRequest {
    std::string student_no;    // 必填
    std::string student_name;  // 必填
    std::string space_id;      // 必填（业务去重键，7.8 §4）
    std::string space_name;    // 必填
    std::string arrival_time;  // 必填
    std::string event_id;      // 可选
    std::string event_type = "arrival";  // 可选，arrival/leave

    static bool fromJson(const nlohmann::json& j,
                         ArrivalNotifyRequest& out,
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
        if (!requireString("student_no", out.student_no) ||
            !requireString("student_name", out.student_name) ||
            !requireString("space_id", out.space_id) ||
            !requireString("space_name", out.space_name) ||
            !requireString("arrival_time", out.arrival_time)) {
            return false;
        }
        if (out.space_id.rfind("spc_", 0) != 0) {
            err = "space_id must start with 'spc_'";
            return false;
        }
        if (j.contains("event_id") && j["event_id"].is_string())
            out.event_id = j["event_id"].get<std::string>();
        if (j.contains("event_type") && j["event_type"].is_string()) {
            out.event_type = j["event_type"].get<std::string>();
            if (out.event_type != "arrival" && out.event_type != "leave") {
                err = "event_type must be 'arrival' or 'leave'";
                return false;
            }
        }
        return true;
    }
};

struct ArrivalNotifyResponse {
    bool accepted = false;
    std::string notify_id;   // == event_id（统一值，便于排查对上）
    std::string final_status;
    std::string channel;     // wechat_oa / none
    std::string reason;      // no_parent_binding / channel_disabled / leave_disabled /
                             // already_notified_today / not_configured / send_failed / ""
    bool db_error = false;       // 幂等查询失败（可重试）；不进 toJson，边侧按 HTTP 状态判断
    bool config_error = false;   // 渠道未配置（真实模式但模板/凭证缺失）；不进 toJson

    nlohmann::json toJson() const {
        return nlohmann::json{{"accepted", accepted},
                              {"notify_id", notify_id},
                              {"final_status", final_status},
                              {"channel", channel},
                              {"reason", reason}};
    }
};

// 到校通知响应 → HTTP 状态码：幂等查库失败(db_error，可重试) 或渠道未配置
// (config_error) → 503，否则 200。控制器据此定 HTTP 状态，作为生产路径唯一
// 来源，避免两处硬编码漂移。
inline int arrivalNotifyHttpStatus(const ArrivalNotifyResponse& r) {
    return (r.db_error || r.config_error) ? 503 : 200;
}
