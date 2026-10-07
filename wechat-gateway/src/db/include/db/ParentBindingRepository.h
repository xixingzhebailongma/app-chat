#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

// parent_student_bindings 表的一行（sql/migration_v1.sql，设计文档
// 十一）。一个学生可绑定多个家长。
struct ParentBinding {
    std::string student_no;
    std::string parent_openid_oa;
    std::string phone;
    std::string verified_at;  // 绑定时间（DB 列 verified_at），响应中映射为 bound_at
    std::string reach_status = "active";  // active / unsubscribed / refused（公众号可达性）
};

// 将学生映射到其家长的公众号 openid。由绑定流程写入
// （POST /api/oa/bind/confirm），由到校推送读取（十一 到校推送），
// 由绑定管理读取/删除（GET /api/oa/bind/me、POST /api/oa/bind/unbind）。
class ParentBindingRepository {
public:
    virtual ~ParentBindingRepository() = default;

    // 绑定到 `student_no` 的所有可达家长的 OA openid（reach_status='active'，
    // 已取关/拒收的跳过；没有则为空）。
    virtual std::vector<std::string> findParentOpenidsByStudentNo(
        const std::string& student_no) const = 0;

    // 绑定到 `openid` 的所有孩子（多孩子模型，没有则为空）。
    virtual std::vector<ParentBinding> findBindingsByOpenid(
        const std::string& parent_openid_oa) const = 0;

    // 幂等写入（upsert）(student_no, parent_openid_oa) -> 绑定。
    virtual void save(const ParentBinding& binding) = 0;

    // 删除 (parent_openid_oa, student_no) 绑定，成功删除返回 true。
    virtual bool remove(const std::string& parent_openid_oa,
                        const std::string& student_no) = 0;

    // 标记家长公众号可达性：43004 未关注 → unsubscribed，43101 拒收 → refused。
    virtual void updateReachStatus(const std::string& parent_openid_oa,
                                   const std::string& student_no,
                                   const std::string& status) = 0;
};

// 内存 mock；真实落库见 PgParentBindingRepository。测试通过 add() 播种。
class InMemoryParentBindingRepository : public ParentBindingRepository {
public:
    void add(const ParentBinding& binding) {
        byKey_[{binding.student_no, binding.parent_openid_oa}] = binding;
    }

    std::vector<std::string> findParentOpenidsByStudentNo(
        const std::string& student_no) const override {
        std::vector<std::string> out;
        for (const auto& [key, binding] : byKey_) {
            if (key.first == student_no && binding.reach_status == "active") {
                out.push_back(binding.parent_openid_oa);
            }
        }
        return out;
    }

    std::vector<ParentBinding> findBindingsByOpenid(
        const std::string& parent_openid_oa) const override {
        std::vector<ParentBinding> out;
        for (const auto& [key, binding] : byKey_) {
            if (binding.parent_openid_oa == parent_openid_oa) {
                out.push_back(binding);
            }
        }
        return out;
    }

    void save(const ParentBinding& binding) override {
        const auto key = std::make_pair(binding.student_no,
                                        binding.parent_openid_oa);
        auto it = byKey_.find(key);
        if (it != byKey_.end()) {
            // 重绑视为重新订阅：重置可达性为 active。
            it->second.phone = binding.phone;
            it->second.reach_status = "active";
            return;
        }
        byKey_.emplace(key, binding);
    }

    bool remove(const std::string& parent_openid_oa,
                const std::string& student_no) override {
        return byKey_.erase({student_no, parent_openid_oa}) > 0;
    }

    void updateReachStatus(const std::string& parent_openid_oa,
                           const std::string& student_no,
                           const std::string& status) override {
        auto it = byKey_.find({student_no, parent_openid_oa});
        if (it != byKey_.end()) {
            it->second.reach_status = status;
        }
    }

private:
    // (student_no, parent_openid_oa) -> 绑定行（一个学生，多个家长）。
    std::map<std::pair<std::string, std::string>, ParentBinding> byKey_;
};
