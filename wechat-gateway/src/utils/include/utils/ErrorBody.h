#pragma once

#include <string>

#include "utils/ErrorCodeMapper.h"
#include "utils/ErrorEnvelope.h"

// 统一错误信封体（配置驱动）：组合 ErrorCodeMapper（码表）+ ErrorEnvelope（信封形状）。
// 各 Result 类型与 http_util 都用这里，保证错误信封只有一个出口。
namespace http_util {

// 显式字符串码 -> 错误体。
inline nlohmann::json errorBody(const std::string& code,
                                const std::string& message) {
    return ErrorEnvelope::instance().wrapError(code, message);
}

// HTTP 状态 -> 规范字符串码 -> 错误体。
inline nlohmann::json errorBodyForStatus(int status,
                                         const std::string& message) {
    return ErrorEnvelope::instance().wrapError(
        ErrorCodeMapper::instance().codeForHttp(status), message);
}

// 成功体（默认原样返回；flat 形态下包裹 code/message/data）。
inline nlohmann::json successBody(const nlohmann::json& body) {
    return ErrorEnvelope::instance().wrapSuccess(body);
}

}  // namespace http_util
