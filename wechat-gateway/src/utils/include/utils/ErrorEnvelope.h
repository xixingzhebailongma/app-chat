#pragma once

#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>
#include <trantor/utils/Logger.h>

// 错误响应信封形状（config/error_envelope.json）。
// 文件缺失 -> 内置默认（嵌套 {"error":{"code","message"}} + 成功原样返回）；
// 非法 JSON -> 抛 std::runtime_error（调用方 LOG_FATAL），绝不静默走错格式。
class ErrorEnvelope {
public:
    static ErrorEnvelope& instance() {
        static ErrorEnvelope e;
        return e;
    }

    void loadFromFile(const std::string& path) {
        std::ifstream in(path);
        if (!in.good()) {
            LOG_WARN << "error_envelope.json missing (" << path
                     << "); using built-in defaults";
            return;
        }
        nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
        if (j.is_discarded() || !j.is_object()) {
            throw std::runtime_error("error_envelope.json is not a valid JSON object");
        }
        if (j.contains("error") && j["error"].is_object()) {
            const auto& e = j["error"];
            errWrapper_ = e.value("wrapper", "error");
            errCodeKey_ = e.value("code_key", "code");
            errMessageKey_ = e.value("message_key", "message");
        }
        if (j.contains("success") && j["success"].is_object()) {
            const auto& s = j["success"];
            succWrapper_ = s.value("wrapper", "");
            succCodeKey_ = s.value("code_key", "code");
            succMessageKey_ = s.value("message_key", "message");
            succCodeValue_ = s.value("code_value", 0);
        }
    }

    // 错误体：wrapper 非空 -> {"<wrapper>":{"code":..,"message":..}}；空 -> 平铺。
    nlohmann::json wrapError(const std::string& code,
                             const std::string& message) const {
        nlohmann::json body{{errCodeKey_, code}, {errMessageKey_, message}};
        if (errWrapper_.empty()) {
            return body;
        }
        return nlohmann::json{{errWrapper_, std::move(body)}};
    }

    // 成功体：wrapper 非空 -> {"code":0,"message":"ok","<wrapper>":<body>}；空 -> 原样返回。
    nlohmann::json wrapSuccess(const nlohmann::json& body) const {
        if (succWrapper_.empty()) {
            return body;
        }
        return nlohmann::json{{succCodeKey_, succCodeValue_},
                              {succMessageKey_, "ok"},
                              {succWrapper_, body}};
    }

private:
    ErrorEnvelope() = default;

    std::string errWrapper_ = "error";
    std::string errCodeKey_ = "code";
    std::string errMessageKey_ = "message";
    std::string succWrapper_ = "";
    std::string succCodeKey_ = "code";
    std::string succMessageKey_ = "message";
    int succCodeValue_ = 0;
};
