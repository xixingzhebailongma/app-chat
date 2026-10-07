#include "services/SubscriptionService.h"

#include <nlohmann/json.hpp>

#include <utility>

SubscriptionService::SubscriptionService(
    std::shared_ptr<SubscriptionRepository> subs,
    std::set<std::string> knownTemplateIds,
    std::set<std::string> longTermTemplateIds)
    : subs_(std::move(subs)),
      knownTemplateIds_(std::move(knownTemplateIds)),
      longTermTemplateIds_(std::move(longTermTemplateIds)) {}

ServiceResult SubscriptionService::applyGrants(
    const std::string& userId,
    const std::vector<std::string>& acceptedTemplates) {
    for (const auto& tid : acceptedTemplates) {
        if (knownTemplateIds_.count(tid) == 0) {
            return ServiceResult::error(drogon::k400BadRequest,
                                        "unknown template_id: " + tid);
        }
    }
    for (const auto& tid : acceptedTemplates) {
        subs_->grant(userId, tid, longTermTemplateIds_.count(tid) > 0);
    }
    return state(userId);
}

ServiceResult SubscriptionService::unsubscribeAll(const std::string& userId) {
    subs_->revokeAll(userId);
    return state(userId);
}

ServiceResult SubscriptionService::state(const std::string& userId) {
    nlohmann::json templates = nlohmann::json::object();
    for (const auto& tid : knownTemplateIds_) {
        const int q = subs_->quotaOf(userId, tid);
        templates[tid] = {{"active", q != 0}, {"quota", q}};
    }
    return ServiceResult::ok(
        nlohmann::json{{"user_id", userId}, {"templates", templates}});
}

ServiceResult SubscriptionService::templateIds() {
    // knownTemplateIds_ 是 std::set，天然去重 + 升序，输出确定性。
    nlohmann::json ids = nlohmann::json::array();
    for (const auto& tid : knownTemplateIds_) {
        ids.push_back(tid);
    }
    return ServiceResult::ok(nlohmann::json{{"template_ids", ids}});
}
