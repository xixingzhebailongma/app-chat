#include "services/SpaceTypeService.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <utility>

SpaceTypeService::SpaceTypeService(std::shared_ptr<SpaceTypeRepository> repo)
    : repo_(std::move(repo)) {}

ServiceResult SpaceTypeService::list() {
    auto types = repo_->listAll();
    // 双保险排序：Pg 已按 sort_order 排，InMemory 未排。
    std::sort(types.begin(), types.end(),
              [](const SpaceType& a, const SpaceType& b) {
                  return a.sort_order < b.sort_order;
              });

    nlohmann::json arr = nlohmann::json::array();
    for (const auto& t : types) {
        arr.push_back({{"code", t.code},
                       {"name", t.name},
                       {"sort_order", t.sort_order},
                       {"enabled", t.enabled},
                       {"icon", t.icon}});
    }
    return ServiceResult::ok(nlohmann::json{{"types", arr}});
}

ServiceResult SpaceTypeService::create(const std::string& role,
                                       const std::string& code,
                                       const std::string& name,
                                       const std::string& icon,
                                       std::optional<int> sortOrder) {
    if (role != "admin") {
        return ServiceResult::error(drogon::k403Forbidden,
                                    "admin role required");
    }
    if (code.empty() || name.empty()) {
        return ServiceResult::error(drogon::k400BadRequest,
                                    "code and name are required");
    }
    if (repo_->findByCode(code)) {
        return ServiceResult::error(drogon::k409Conflict,
                                    "code already exists");
    }

    SpaceType t;
    t.code = code;
    t.name = name;
    t.icon = icon;
    t.enabled = true;
    // sort_order 缺省排最后（max+1），显式传则用传值。
    t.sort_order = sortOrder.has_value() ? *sortOrder
                                         : repo_->maxSortOrder() + 1;
    repo_->upsert(t);
    return ServiceResult::ok(nlohmann::json{{"ok", true}});
}

ServiceResult SpaceTypeService::update(const std::string& role,
                                       const std::string& code,
                                       std::optional<std::string> name,
                                       std::optional<std::string> icon,
                                       std::optional<bool> enabled) {
    if (role != "admin") {
        return ServiceResult::error(drogon::k403Forbidden,
                                    "admin role required");
    }
    const auto existing = repo_->findByCode(code);
    if (!existing) {
        return ServiceResult::error(drogon::k404NotFound,
                                    "space type not found");
    }

    SpaceType t = *existing;
    if (name.has_value()) {
        t.name = *name;
    }
    if (icon.has_value()) {
        t.icon = *icon;
    }
    if (enabled.has_value()) {
        t.enabled = *enabled;
    }
    repo_->upsert(t);
    return ServiceResult::ok(nlohmann::json{{"ok", true}});
}

ServiceResult SpaceTypeService::disable(const std::string& role,
                                        const std::string& code) {
    if (role != "admin") {
        return ServiceResult::error(drogon::k403Forbidden,
                                    "admin role required");
    }
    if (!repo_->findByCode(code)) {
        return ServiceResult::error(drogon::k404NotFound,
                                    "space type not found");
    }
    repo_->disable(code);
    return ServiceResult::ok(nlohmann::json{{"ok", true}});
}

ServiceResult SpaceTypeService::reorder(
    const std::string& role, const std::vector<std::string>& codes) {
    if (role != "admin") {
        return ServiceResult::error(drogon::k403Forbidden,
                                    "admin role required");
    }
    repo_->setSortOrder(codes);
    return ServiceResult::ok(nlohmann::json{{"ok", true}});
}
