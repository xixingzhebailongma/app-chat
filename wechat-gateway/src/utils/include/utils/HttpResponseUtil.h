#pragma once

#include <drogon/HttpResponse.h>
#include <nlohmann/json.hpp>
#include <atomic>
#include <chrono>
#include <string>

#include "utils/ErrorBody.h"

namespace http_util {

inline drogon::HttpResponsePtr jsonResponse(drogon::HttpStatusCode code,
                                            const nlohmann::json& body) {
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(code);
    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    resp->setBody(body.dump());
    return resp;
}

// 统一错误体（配置驱动，见 config/error_envelope.json + error_codes.json）。
inline drogon::HttpResponsePtr error(drogon::HttpStatusCode code,
                                     const std::string& message) {
    return jsonResponse(code,
                        errorBodyForStatus(static_cast<int>(code), message));
}

// 带显式机器可读字符串代码的错误（如 "GO_BACKEND_UNAVAILABLE"）。
inline drogon::HttpResponsePtr errorWithCode(drogon::HttpStatusCode code,
                                             const std::string& errCode,
                                             const std::string& message) {
    return jsonResponse(code, errorBody(errCode, message));
}

inline drogon::HttpResponsePtr ok(const nlohmann::json& body) {
    return jsonResponse(drogon::k200OK, successBody(body));
}

// 构建一个原样镜像 go-backend 响应的响应——状态码、
// 内容类型和响应体（设计文档 八 原样返回响应）。
inline drogon::HttpResponsePtr passthrough(drogon::HttpStatusCode code,
                                           drogon::ContentType contentType,
                                           const std::string& body) {
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(code);
    resp->setContentTypeCode(contentType);
    resp->setBody(body);
    return resp;
}

// 抗冲突 id：前缀 + 纪元毫秒 + 原子序号。
inline std::string newId(const std::string& prefix) {
    static std::atomic<uint64_t> seq{0};
    auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::system_clock::now().time_since_epoch())
                     .count();
    return prefix + "_" + std::to_string(nowMs) + "_" +
           std::to_string(seq.fetch_add(1));
}

}  // namespace http_util
