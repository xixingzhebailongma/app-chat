#pragma once

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <string>

// POST /api/oa/bind/confirm 的请求 DTO（设计文档 十一 绑定流程）。

struct OaSendCodeRequest {
    std::string phone;  // 必填；11 位中国大陆手机号

    static bool fromJson(const nlohmann::json& j,
                         OaSendCodeRequest& out,
                         std::string& err) {
        if (!j.is_object()) {
            err = "body must be a JSON object";
            return false;
        }
        if (!j.contains("phone") || !j["phone"].is_string() ||
            j["phone"].get<std::string>().empty()) {
            err = "phone is required";
            return false;
        }
        out.phone = j["phone"].get<std::string>();
        if (out.phone.size() != 11 ||
            !std::all_of(out.phone.begin(), out.phone.end(),
                         [](unsigned char c) { return std::isdigit(c); })) {
            err = "phone must be an 11-digit mobile number";
            return false;
        }
        return true;
    }
};

struct OaBindConfirmRequest {
    std::string openid_token;  // 由 GET /api/oa/bind/authorize 签发
    std::string student_no;    // 必填
    std::string phone;         // 必填
    std::string sms_code;      // 必填

    static bool fromJson(const nlohmann::json& j,
                         OaBindConfirmRequest& out,
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
            !requireString("student_no", out.student_no) ||
            !requireString("phone", out.phone) ||
            !requireString("sms_code", out.sms_code)) {
            return false;
        }
        return true;
    }
};

// POST /api/oa/bind/unbind 的请求 DTO（设计文档 十一 绑定管理）。
struct OaUnbindRequest {
    std::string openid_token;  // 由 GET /api/oa/bind/authorize 签发
    std::string student_no;    // 必填，要解绑的学号

    static bool fromJson(const nlohmann::json& j,
                         OaUnbindRequest& out,
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
            !requireString("student_no", out.student_no)) {
            return false;
        }
        return true;
    }
};
