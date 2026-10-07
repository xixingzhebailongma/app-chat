#pragma once

#include <nlohmann/json.hpp>
#include <string>

// 小程序鉴权接口的请求 DTO（设计文档 六）。

struct MiniappLoginRequest {
    std::string code;  // wx.login() 的 code，必填

    static bool fromJson(const nlohmann::json& j,
                         MiniappLoginRequest& out,
                         std::string& err) {
        if (!j.is_object()) {
            err = "body must be a JSON object";
            return false;
        }
        if (!j.contains("code") || !j["code"].is_string() ||
            j["code"].get<std::string>().empty()) {
            err = "code is required";
            return false;
        }
        out.code = j["code"].get<std::string>();
        return true;
    }
};

struct MiniappBindRequest {
    std::string openid_token;  // 来自 /login 的短期票据，必填
    std::string username;      // 必填
    std::string password;      // 必填

    static bool fromJson(const nlohmann::json& j,
                         MiniappBindRequest& out,
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
        if (!requireString("openid_token", out.openid_token) ||
            !requireString("username", out.username) ||
            !requireString("password", out.password)) {
            return false;
        }
        return true;
    }
};
