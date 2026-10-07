#pragma once

#include <drogon/HttpTypes.h>
#include <nlohmann/json.hpp>

#include <string>
#include <utility>

#include "utils/ErrorBody.h"

// 同步（内存）服务返回的结果：一个 HTTP 状态码
// 加上一个可直接发送的 JSON 体（成功负载或统一错误
// 信封）。镜像异步鉴权流程中使用的 AuthResult 结构。
struct ServiceResult {
    drogon::HttpStatusCode status = drogon::k200OK;
    nlohmann::json body;

    static ServiceResult ok(nlohmann::json b) {
        ServiceResult r;
        r.status = drogon::k200OK;
        r.body = std::move(b);
        return r;
    }

    static ServiceResult error(drogon::HttpStatusCode s,
                               const std::string& msg) {
        ServiceResult r;
        r.status = s;
        r.body = http_util::errorBodyForStatus(static_cast<int>(s), msg);
        return r;
    }
};
