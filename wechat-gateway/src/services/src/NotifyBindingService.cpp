#include "services/NotifyBindingService.h"

#include <drogon/HttpTypes.h>
#include <nlohmann/json.hpp>

#include <set>
#include <string>
#include <utility>

namespace {

const std::set<std::string> kKnownChannels = {"wechat_miniapp", "sms",
                                              "dingtalk", "wecom"};

}  // namespace

NotifyBindingService::NotifyBindingService(
    std::shared_ptr<NotifyBindingRepository> repo)
    : repo_(std::move(repo)) {}

ServiceResult NotifyBindingService::list(const std::string& role,
                                         const std::string& channel) {
    if (role != "admin") {
        return ServiceResult::error(drogon::k403Forbidden,
                                    "admin role required");
    }
    if (channel.empty()) {
        return ServiceResult::error(drogon::k400BadRequest,
                                    "channel required");
    }
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& b : repo_->list(channel)) {
        arr.push_back({{"user_id", b.user_id},
                       {"channel", b.channel},
                       {"external_id", b.external_id},
                       {"updated_at", b.updated_at}});
    }
    return ServiceResult::ok(nlohmann::json{{"bindings", arr}});
}

ServiceResult NotifyBindingService::save(const std::string& role,
                                         const std::string& channel,
                                         const std::string& user_id,
                                         const std::string& external_id,
                                         const std::string& external_extra) {
    if (role != "admin") {
        return ServiceResult::error(drogon::k403Forbidden,
                                    "admin role required");
    }
    if (kKnownChannels.count(channel) == 0) {
        return ServiceResult::error(drogon::k400BadRequest,
                                    "unknown channel: " + channel);
    }
    if (user_id.empty() || external_id.empty()) {
        return ServiceResult::error(drogon::k400BadRequest,
                                    "user_id and external_id are required");
    }
    NotifyBinding b{channel, user_id, external_id, external_extra};
    if (!repo_->save(b)) {
        return ServiceResult::error(drogon::k500InternalServerError,
                                    "save failed");
    }
    return ServiceResult::ok(nlohmann::json{{"ok", true}});
}

ServiceResult NotifyBindingService::remove(const std::string& role,
                                           const std::string& channel,
                                           const std::string& user_id) {
    if (role != "admin") {
        return ServiceResult::error(drogon::k403Forbidden,
                                    "admin role required");
    }
    if (channel.empty() || user_id.empty()) {
        return ServiceResult::error(drogon::k400BadRequest,
                                    "channel and user_id are required");
    }
    repo_->remove(channel, user_id);
    return ServiceResult::ok(nlohmann::json{{"ok", true}});
}
