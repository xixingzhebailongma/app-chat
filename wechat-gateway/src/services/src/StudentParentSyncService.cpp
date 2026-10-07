#include "services/StudentParentSyncService.h"

#include <nlohmann/json.hpp>

#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

StudentParentSyncService::StudentParentSyncService(
    std::shared_ptr<StudentParentRepository> parents)
    : parents_(std::move(parents)) {}

ServiceResult StudentParentSyncService::sync(
    const std::vector<StudentParent>& rows) {
    // 按 (student_no, parent_phone) 去重（空键忽略，防脏数据），构造本轮应存在的集合。
    // 复合键用 \x1f 分隔，避免学号/手机号里出现分隔符造成串键。
    auto key = [](const std::string& student_no, const std::string& parent_phone) {
        return student_no + "\x1f" + parent_phone;
    };

    std::unordered_set<std::string> provided;
    std::vector<StudentParent> toUpsert;
    toUpsert.reserve(rows.size());
    for (const auto& r : rows) {
        if (r.student_no.empty() || r.parent_phone.empty()) {
            continue;
        }
        if (provided.insert(key(r.student_no, r.parent_phone)).second) {
            toUpsert.push_back(r);
        }
    }

    // 现存行集合，用于计算「应删除」的差集。
    const std::vector<StudentParent> existing = parents_->listAll();
    std::unordered_set<std::string> current;
    current.reserve(existing.size());
    for (const auto& r : existing) {
        current.insert(key(r.student_no, r.parent_phone));
    }

    // 1) upsert 给定项。
    for (const auto& r : toUpsert) {
        parents_->upsert(r);
    }

    // 2) 删除缺失项。
    int removed = 0;
    for (const auto& r : existing) {
        if (provided.count(key(r.student_no, r.parent_phone)) == 0) {
            parents_->remove(r.student_no, r.parent_phone);
            ++removed;
        }
    }

    return ServiceResult::ok(nlohmann::json{
        {"ok", true}, {"upserted", toUpsert.size()}, {"removed", removed}});
}
