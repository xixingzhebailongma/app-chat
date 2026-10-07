#pragma once

#include <fstream>
#include <map>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>
#include <trantor/utils/Logger.h>

// 网关错误码映射（config/error_codes.json）。
// 只做「字符串码 ↔ HTTP 状态」映射（不引入数字码）。内置默认等价于现状
// （改造前硬编码映射）。查不到码时回落 INTERNAL_ERROR + 500 并 LOG_WARN。
class ErrorCodeMapper {
public:
    static ErrorCodeMapper& instance() {
        static ErrorCodeMapper m;
        return m;
    }

    // 文件缺失 -> 保留内置默认；非法 JSON -> 抛 std::runtime_error（调用方 LOG_FATAL）。
    void loadFromFile(const std::string& path) {
        std::ifstream in(path);
        if (!in.good()) {
            LOG_WARN << "error_codes.json missing (" << path
                     << "); using built-in defaults";
            return;
        }
        nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
        if (j.is_discarded() || !j.is_object()) {
            throw std::runtime_error("error_codes.json is not a valid JSON object");
        }
        if (j.contains("codes") && j["codes"].is_object()) {
            for (auto it = j["codes"].begin(); it != j["codes"].end(); ++it) {
                if (it.value().is_number_integer()) {
                    codeToHttp_[it.key()] = it.value().get<int>();
                }
            }
        }
        if (j.contains("fallback") && j["fallback"].is_object()) {
            fallbackCode_ = j["fallback"].value("code", "INTERNAL_ERROR");
            fallbackHttp_ = j["fallback"].value("http", 500);
        }
    }

    // 字符串码 -> HTTP 状态；未知码回落 fallback 并打 warn。
    int httpForCode(const std::string& code) const {
        auto it = codeToHttp_.find(code);
        if (it == codeToHttp_.end()) {
            LOG_WARN << "error_codes: unknown code '" << code
                     << "', fallback to " << fallbackCode_ << "/" << fallbackHttp_;
            return fallbackHttp_;
        }
        return it->second;
    }

    // HTTP 状态 -> 规范字符串码（http_util::error 用）；未知状态回落 fallback code。
    std::string codeForHttp(int status) const {
        for (const auto& [code, http] : codeToHttp_) {
            if (http == status) {
                return code;
            }
        }
        return fallbackCode_;
    }

private:
    ErrorCodeMapper() {
        codeToHttp_ = {
            {"INVALID_REQUEST", 400},
            {"UNAUTHORIZED", 401},
            {"FORBIDDEN", 403},
            {"NOT_FOUND", 404},
            {"CONFLICT", 409},
            {"CONFIRM_REQUIRED", 428},
            {"RATE_LIMITED", 429},
            {"INTERNAL_ERROR", 500},
            {"UPSTREAM_ERROR", 502},
            {"UPSTREAM_UNAVAILABLE", 503},
            {"GO_BACKEND_UNAVAILABLE", 503},
        };
        fallbackCode_ = "INTERNAL_ERROR";
        fallbackHttp_ = 500;
    }

    std::map<std::string, int> codeToHttp_;
    std::string fallbackCode_;
    int fallbackHttp_;
};
