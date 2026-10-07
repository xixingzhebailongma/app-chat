#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// 解析通知的收件人 user_id（设计文档 9.4 第 2 步）：
// 所有拥有某个角色的用户（全局）以及绑定到某空间的教师。
// TODO: 使用 PgPool 实现基于 PostgreSQL 的仓库（角色数据在
// go-backend 中；教师是 user_spaces 表中的行）。
class NotifyTargetRepository {
public:
    virtual ~NotifyTargetRepository() = default;

    virtual std::vector<std::string> usersByRole(
        const std::string& role) const = 0;

    virtual std::vector<std::string> usersBySpace(
        const std::string& space_id) const = 0;
};

// 内存 mock。在测试 / 开发装配中通过 addUser()/addSpaceMember() 播种数据。
class InMemoryNotifyTargetRepository : public NotifyTargetRepository {
public:
    void addUser(const std::string& user_id, const std::string& role) {
        byRole_[role].insert(user_id);
    }

    void addSpaceMember(const std::string& space_id,
                        const std::string& user_id) {
        bySpace_[space_id].insert(user_id);
    }

    std::vector<std::string> usersByRole(const std::string& role) const override {
        std::vector<std::string> out;
        const auto it = byRole_.find(role);
        if (it != byRole_.end()) {
            out.assign(it->second.begin(), it->second.end());
        }
        return out;
    }

    std::vector<std::string> usersBySpace(
        const std::string& space_id) const override {
        std::vector<std::string> out;
        const auto it = bySpace_.find(space_id);
        if (it != bySpace_.end()) {
            out.assign(it->second.begin(), it->second.end());
        }
        return out;
    }

private:
    std::unordered_map<std::string, std::unordered_set<std::string>> byRole_;
    std::unordered_map<std::string, std::unordered_set<std::string>> bySpace_;
};
