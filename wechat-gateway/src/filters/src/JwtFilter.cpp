#include "filters/JwtFilter.h"

#include <drogon/HttpRequest.h>

#include <utility>

#include "utils/HttpResponseUtil.h"
#include "utils/JwtUtil.h"

std::shared_ptr<UserRoleRepository> JwtFilter::userRoles_;
int JwtFilter::roleCacheTtlSeconds_ = 60;
std::mutex JwtFilter::cacheMutex_;
std::unordered_map<std::string, JwtFilter::RoleEntry> JwtFilter::roleCache_;

void JwtFilter::configure(std::shared_ptr<UserRoleRepository> roles,
                          int roleCacheTtlSeconds) {
    userRoles_ = std::move(roles);
    if (roleCacheTtlSeconds > 0) {
        roleCacheTtlSeconds_ = roleCacheTtlSeconds;
    }
}

std::string JwtFilter::cachedRoleOf(const std::string& user_id) {
    if (!userRoles_) {
        return "";
    }
    const auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        const auto it = roleCache_.find(user_id);
        if (it != roleCache_.end() &&
            now - it->second.at < std::chrono::seconds(roleCacheTtlSeconds_)) {
            return it->second.role;
        }
    }
    // 缓存未命中：同步查 user_roles（PG 单连接 + 互斥，与其余仓库一致）。
    const std::string role = userRoles_->roleOf(user_id);
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        roleCache_[user_id] = RoleEntry{role, now};
    }
    return role;
}

void JwtFilter::doFilter(const drogon::HttpRequestPtr& req,
                         drogon::FilterCallback&& fcb,
                         drogon::FilterChainCallback&& fccb) {
    const std::string auth = req->getHeader("Authorization");
    const std::string prefix = "Bearer ";
    if (auth.size() <= prefix.size() ||
        auth.compare(0, prefix.size(), prefix) != 0) {
        fcb(http_util::error(drogon::k401Unauthorized,
                             "missing or invalid Authorization header"));
        return;
    }

    const auto claims = JwtUtil::verifyAccessToken(auth.substr(prefix.size()));
    if (!claims) {
        fcb(http_util::error(drogon::k401Unauthorized,
                             "invalid or expired token"));
        return;
    }

    (*req->getAttributes())["user_id"] = claims->user_id;
    (*req->getAttributes())["role"] = claims->role;

    // 实时角色覆盖：user_roles 非空优先，否则保留 JWT 内嵌 role。
    const std::string freshRole = cachedRoleOf(claims->user_id);
    if (!freshRole.empty()) {
        (*req->getAttributes())["role"] = freshRole;
    }

    fccb();
}
