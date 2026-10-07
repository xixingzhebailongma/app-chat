#pragma once

#include <drogon/HttpFilter.h>

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "db/UserRoleRepository.h"

// 依据共享密钥本地校验 `Authorization: Bearer {jwt}`，并将解码后的
// 身份存入请求属性（设计文档 六 "后续请求"）。
//
// 角色实时性（7.7 收尾项 d）：验签后按 user_id 查 user_roles 的实时角色，
// 非空则覆盖 JWT 内嵌 role，使角色变更无需重新登录即可生效。user_roles
// 无记录时回退到 JWT 内嵌 role（与旧行为兼容）。查库带短 TTL 进程内缓存
// （默认 60s），避免每请求一次 PG 查询。
class JwtFilter : public drogon::HttpFilter<JwtFilter> {
public:
    void doFilter(const drogon::HttpRequestPtr& req,
                  drogon::FilterCallback&& fcb,
                  drogon::FilterChainCallback&& fccb) override;

    // 由 main() 在首个请求前配置（与 InternalTokenFilter::setToken 同模式）。
    // roles 为空 = 不启用实时角色覆盖，仅用 JWT 内嵌 role。
    static void configure(std::shared_ptr<UserRoleRepository> roles,
                          int roleCacheTtlSeconds = 60);

private:
    static std::string cachedRoleOf(const std::string& user_id);

    static std::shared_ptr<UserRoleRepository> userRoles_;
    static int roleCacheTtlSeconds_;
    static std::mutex cacheMutex_;

    struct RoleEntry {
        std::string role;
        std::chrono::steady_clock::time_point at;
    };
    static std::unordered_map<std::string, RoleEntry> roleCache_;
};
