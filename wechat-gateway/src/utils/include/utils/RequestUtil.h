#pragma once

#include <drogon/HttpRequest.h>

#include <any>
#include <string>

namespace request_util {

// 由 JwtFilter 解码并存入请求属性的身份（设计文档
// 六 后续请求）。未应用该过滤器时 user_id/role 为空。
struct RequestIdentity {
    std::string user_id;
    std::string role;
};

inline RequestIdentity identity(const drogon::HttpRequestPtr& req) {
    RequestIdentity id;
    const auto attrs = req->getAttributes();
    id.user_id = attrs->get<std::string>("user_id");
    id.role = attrs->get<std::string>("role");
    return id;
}

// 来自 Authorization 头的原始 bearer 令牌（缺失则为空），
// 用于作为调用方的 JWT 转发给 go-backend（设计文档 八 转发）。
inline std::string bearerToken(const drogon::HttpRequestPtr& req) {
    const std::string auth = req->getHeader("Authorization");
    const std::string prefix = "Bearer ";
    if (auth.compare(0, prefix.size(), prefix) == 0) {
        return auth.substr(prefix.size());
    }
    return "";
}

}  // namespace request_util
