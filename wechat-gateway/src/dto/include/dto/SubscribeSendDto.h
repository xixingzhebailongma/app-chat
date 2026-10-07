#pragma once

#include <nlohmann/json.hpp>

#include <map>
#include <string>

// POST /internal/notify/subscribe-send 的 DTO —— 微信订阅消息真实发送
// 的最小触发接口：给定 openid + 模板 ID + 模板数据，直接调微信 API。

struct SubscribeSendRequest {
    std::string openid;       // 必填：接收者 openid（touser）
    std::string template_id;  // 必填：订阅消息模板 ID
    std::map<std::string, std::string> data;  // 必填：模板 data（key -> 字符串 value）
    std::string page;         // 可选：点击消息跳转的小程序页面路径
    std::string miniprogram_state = "formal";  // 可选：developer/trial/formal

    static bool fromJson(const nlohmann::json& j,
                         SubscribeSendRequest& out,
                         std::string& err) {
        if (!j.is_object()) {
            err = "body must be a JSON object";
            return false;
        }
        if (!j.contains("openid") || !j["openid"].is_string() ||
            j["openid"].get<std::string>().empty()) {
            err = "openid is required";
            return false;
        }
        out.openid = j["openid"].get<std::string>();

        if (!j.contains("template_id") || !j["template_id"].is_string() ||
            j["template_id"].get<std::string>().empty()) {
            err = "template_id is required";
            return false;
        }
        out.template_id = j["template_id"].get<std::string>();

        if (!j.contains("data") || !j["data"].is_object()) {
            err = "data is required and must be an object";
            return false;
        }
        for (auto it = j["data"].begin(); it != j["data"].end(); ++it) {
            if (!it.value().is_string()) {
                err = "data values must be strings";
                return false;
            }
            out.data[it.key()] = it.value().get<std::string>();
        }

        if (j.contains("page") && j["page"].is_string())
            out.page = j["page"].get<std::string>();
        if (j.contains("miniprogram_state") &&
            j["miniprogram_state"].is_string())
            out.miniprogram_state = j["miniprogram_state"].get<std::string>();

        return true;
    }
};
