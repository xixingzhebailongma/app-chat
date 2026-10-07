#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// user_spaces 表的一行（见 sql/migration_v1.sql）：一个 (user_id,
// space_id) 成员关系，用于校验教师对某空间的访问权限。
// TODO: 使用 PgPool 实现基于 PostgreSQL 的仓库。
class UserSpacesRepository {
public:
    virtual ~UserSpacesRepository() = default;

    virtual bool contains(const std::string& user_id,
                          const std::string& space_id) const = 0;

    // 用户绑定的所有 space_id（没有则为空）。
    virtual std::vector<std::string> spacesForUser(
        const std::string& user_id) const = 0;

    // 绑定 / 解绑（幂等）。add 已存在则 no-op；remove 不存在则 no-op。
    virtual void add(const std::string& user_id,
                     const std::string& space_id) = 0;
    virtual void remove(const std::string& user_id,
                        const std::string& space_id) = 0;

    // 删除某空间下的全部绑定（空间生命周期同步的级联清理；幂等）。
    virtual void removeBySpace(const std::string& space_id) = 0;
};

// 内存 mock。在测试 / 开发装配中通过 add() 播种数据；生产环境在
// 同一接口后替换为 PostgreSQL 实现。
class InMemoryUserSpacesRepository : public UserSpacesRepository {
public:
    void add(const std::string& user_id,
             const std::string& space_id) override {
        byUser_[user_id].insert(space_id);
    }

    void remove(const std::string& user_id,
                const std::string& space_id) override {
        auto it = byUser_.find(user_id);
        if (it != byUser_.end()) {
            it->second.erase(space_id);
        }
    }

    void removeBySpace(const std::string& space_id) override {
        for (auto it = byUser_.begin(); it != byUser_.end();) {
            it->second.erase(space_id);
            if (it->second.empty()) {
                it = byUser_.erase(it);
            } else {
                ++it;
            }
        }
    }

    bool contains(const std::string& user_id,
                  const std::string& space_id) const override {
        const auto it = byUser_.find(user_id);
        return it != byUser_.end() && it->second.count(space_id) > 0;
    }

    std::vector<std::string> spacesForUser(
        const std::string& user_id) const override {
        std::vector<std::string> out;
        const auto it = byUser_.find(user_id);
        if (it != byUser_.end()) {
            out.assign(it->second.begin(), it->second.end());
        }
        return out;
    }

private:
    std::unordered_map<std::string, std::unordered_set<std::string>> byUser_;
};
