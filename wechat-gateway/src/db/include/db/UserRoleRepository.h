#pragma once

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// 角色优先级归一：admin > teacher > 其他首个非空角色；空集合返回空串。
// 供 roleOf（实时角色）在用户多角色时挑选最高权限者。
inline std::string highestRole(const std::vector<std::string>& roles) {
    bool hasTeacher = false;
    std::string other;
    for (const auto& r : roles) {
        if (r == "admin") {
            return "admin";
        }
        if (r == "teacher") {
            hasTeacher = true;
        } else if (other.empty() && !r.empty()) {
            other = r;
        }
    }
    return hasTeacher ? "teacher" : other;
}

// user_roles 表的读写（sql/migration_v3.sql，设计文档 14）。
// 权威源在 go-backend；网关只在 usersByRole 读它，写入由 go-backend 经
// POST /internal/user-roles/sync 触发（全量替换，幂等）。
class UserRoleRepository {
public:
    virtual ~UserRoleRepository() = default;

    // 全量替换 user_id 的角色集合（为空即清除该用户全部角色）。
    virtual void setRoles(const std::string& user_id,
                          const std::vector<std::string>& roles) = 0;

    // 移除该用户的所有角色。
    virtual void removeUser(const std::string& user_id) = 0;

    // 列出拥有该角色的所有 user_id（按 user_id 升序）。
    virtual std::vector<std::string> usersByRole(
        const std::string& role) const = 0;

    // 该用户的最高权限角色（admin > teacher > 其他）；无记录返回空串。
    // 供 JwtFilter 做实时角色覆盖（7.7 收尾项 d）。
    virtual std::string roleOf(const std::string& user_id) const = 0;
};

// 内存 mock（开发/测试）。注意：与 InMemoryNotifyTargetRepository 的
// byRole_ 各自独立——在纯内存模式下同步接口不会反向影响收件人解析，
// 二者只在 PostgreSQL 实现里共享同一张 user_roles 表。
class InMemoryUserRoleRepository : public UserRoleRepository {
public:
    void setRoles(const std::string& user_id,
                  const std::vector<std::string>& roles) override {
        byUser_[user_id] =
            std::unordered_set<std::string>(roles.begin(), roles.end());
    }

    void removeUser(const std::string& user_id) override {
        byUser_.erase(user_id);
    }

    std::vector<std::string> usersByRole(const std::string& role) const override {
        std::vector<std::string> out;
        for (const auto& [user_id, roles] : byUser_) {
            if (roles.count(role) > 0) {
                out.push_back(user_id);
            }
        }
        std::sort(out.begin(), out.end());
        return out;
    }

    std::string roleOf(const std::string& user_id) const override {
        auto it = byUser_.find(user_id);
        if (it == byUser_.end()) {
            return "";
        }
        return highestRole(
            std::vector<std::string>(it->second.begin(), it->second.end()));
    }

private:
    std::unordered_map<std::string, std::unordered_set<std::string>> byUser_;
};
