#include "services/NotifyService.h"

#include <trantor/utils/Logger.h>

#include <exception>
#include <unordered_set>
#include <utility>

#include "db/AlertRepository.h"
#include "db/RedisClient.h"
#include "services/RetryWorker.h"
#include "utils/HttpResponseUtil.h"

NotifyService::NotifyService(
    std::shared_ptr<NotifyLogRepository> logRepo,
    std::shared_ptr<NotifyTargetRepository> targetRepo,
    std::shared_ptr<PendingEventRepository> pendingRepo,
    std::shared_ptr<RedisClient> redis)
    : logRepo_(std::move(logRepo)),
      targetRepo_(std::move(targetRepo)),
      pendingRepo_(std::move(pendingRepo)),
      cooldown_(std::move(redis)) {}

void NotifyService::registerChannel(std::shared_ptr<INotifyChannel> channel) {
    if (channel) {
        channels_[channel->name()] = std::move(channel);
    }
}

void NotifyService::setRouting(
    const std::vector<std::string>& defaultChannels,
    const std::map<std::string, std::vector<std::string>>& byEvent) {
    for (auto& rule : rules_) {
        const auto it = byEvent.find(rule.event_type);
        const auto& src = it != byEvent.end() ? it->second : defaultChannels;

        std::vector<std::string> kept;
        for (const auto& name : src) {
            auto c = channels_.find(name);
            if (c == channels_.end()) {
                LOG_WARN << "routing: unknown channel '" << name << "' for "
                         << rule.event_type << ", dropped";
                continue;
            }
            if (!c->second->enabled()) {
                LOG_WARN << "routing: disabled channel '" << name << "' for "
                         << rule.event_type << ", dropped";
                continue;
            }
            kept.push_back(name);
        }
        rule.channels = std::move(kept);  // 可为空 -> 发送时走兜底
    }
}

const PushRule* NotifyService::findRule(
    const std::string& event_type) const {
    for (const auto& rule : rules_) {
        if (rule.event_type == event_type) {
            return &rule;
        }
    }
    return nullptr;
}

void NotifyService::resolveRecipients(const PushRule& rule,
                                      const NotifySendRequest& req,
                                      std::vector<std::string>& out) const {
    std::unordered_set<std::string> seen;
    if (targetRepo_) {
        for (const auto& role : rule.target_roles) {
            for (const auto& uid : targetRepo_->usersByRole(role)) {
                seen.insert(uid);
            }
        }
        if (rule.to_space_teacher && !req.space_id.empty()) {
            for (const auto& uid : targetRepo_->usersBySpace(req.space_id)) {
                seen.insert(uid);
            }
        }
    }
    out.assign(seen.begin(), seen.end());
}

ChannelPayload NotifyService::buildPayload(const NotifySendRequest& req,
                                           const PushRule& rule) const {
    ChannelPayload p;
    p.event_id = req.event_id;
    p.event_type = req.event_type;
    p.content = req.content;
    p.severity = req.severity;
    p.space_id = req.space_id;
    p.device_id = req.device_id;
    p.device_label = req.device_label;
    // 通知对象的判定以 PushRule 为准，而非请求（设计文档 9.2）。
    p.roles.assign(rule.target_roles.begin(), rule.target_roles.end());
    p.space_teachers = rule.to_space_teacher;
    return p;
}

std::shared_ptr<INotifyChannel> NotifyService::locateFallbackChannel() const {
    if (!fallbackChannel_.empty()) {
        const auto it = channels_.find(fallbackChannel_);
        if (it != channels_.end() && it->second->enabled()) {
            if (!it->second->capabilities().isFallback) {
                LOG_WARN << "fallback channel '" << fallbackChannel_
                         << "' does not declare isFallback; using it anyway "
                            "(config wins)";
            }
            return it->second;
        }
        LOG_WARN << "fallback channel '" << fallbackChannel_
                 << "' not registered or disabled; no fallback";
        return nullptr;
    }
    for (const auto& [name, ch] : channels_) {
        (void)name;
        if (ch->enabled() && ch->capabilities().isFallback) {
            return ch;
        }
    }
    return nullptr;
}

NotifySendResponse NotifyService::send(const NotifySendRequest& req) {
    NotifySendResponse resp;
    resp.notify_id = http_util::newId("ntf");
    resp.accepted = true;

    // 1. 按 event_type 匹配 PushRule。
    const PushRule* rule = findRule(req.event_type);
    if (rule == nullptr) {
        resp.final_status = "skipped";
        return resp;
    }

    // 2. 对 7.6 发送规则做防御性复查（见 PushRule.h）。
    if (rule->condition && !rule->condition(req)) {
        resp.final_status = "skipped";
        return resp;
    }

    // 3. 解析接收人（设计文档 9.4 第 2 步）。为空 -> 无需发送。
    ChannelPayload payload = buildPayload(req, *rule);
    resolveRecipients(*rule, req, payload.recipients);
    if (payload.recipients.empty()) {
        resp.final_status = "skipped";
        return resp;
    }

    // 3.5 告警入库（producer 侧，设计文档 7.5②）。
    if (alertRepo_) {
        if (!req.event_id.empty()) {
            Alert a{req.event_id, req.space_id, req.event_type, "unhandled", "",
                    ""};
            a.device_id = req.device_id;
            a.device_label = req.device_label;
            try {
                if (!alertRepo_->upsert(a)) {
                    LOG_ERROR << "alert upsert failed event=" << req.event_id
                              << " space=" << req.space_id
                              << " type=" << req.event_type;
                    ++alertUpsertFailures_;
                }
            } catch (const std::exception& e) {
                LOG_ERROR << "alert upsert threw event=" << req.event_id
                          << " err=" << e.what();
                ++alertUpsertFailures_;
            }
        } else {
            LOG_ERROR << "alert event_id missing, skip record space="
                      << req.space_id << " type=" << req.event_type;
            ++alertUpsertFailures_;
        }
    }

    // 4. 按用户分派（设计文档 13.3 / 13.6）。
    int cooldownTtl = rule->cooldown_seconds;
    if (cooldownTtl <= 0) {
        cooldownTtl = defaultCooldownSeconds_;
    }

    std::vector<ChannelAttempt> attempts;
    int covered = 0;
    for (const auto& user : payload.recipients) {
        if (dispatchToUser(*rule, req, payload, user, cooldownTtl, attempts)) {
            ++covered;
        }
    }

    // 5. 汇总最终状态。
    if (covered == static_cast<int>(payload.recipients.size())) {
        resp.final_status = "success";
    } else if (covered > 0) {
        resp.final_status = "partial";
    } else {
        resp.final_status = "failed";
    }
    if (attempts.empty() && resp.final_status == "success") {
        resp.final_status = "skipped";
    }

    // channels_tried：实际尝试过的去重渠道，按首次出现顺序排列。
    std::unordered_set<std::string> seenChannels;
    for (const auto& a : attempts) {
        if (seenChannels.insert(a.channel).second) {
            resp.channels_tried.push_back(a.channel);
        }
    }

    // 6. 记录分派（notify_logs + 每 (user,channel) 尝试）。
    if (logRepo_) {
        logRepo_->save(resp.notify_id, req, attempts, resp.final_status);
    }

    // 7. 降级模式：仅在确有送达失败时才挂起以便后台重试。
    if ((resp.final_status == "failed" || resp.final_status == "partial") &&
        pendingRepo_) {
        pendingRepo_->enqueue(req.toJson());
    }

    return resp;
}

bool NotifyService::dispatchToUser(const PushRule& rule,
                                   const NotifySendRequest& req,
                                   const ChannelPayload& payload,
                                   const std::string& user,
                                   int cooldownTtl,
                                   std::vector<ChannelAttempt>& attempts) {
    bool covered = false;
    bool anyPrimaryAttempted = false;

    for (const auto& name : rule.channels) {
        const auto it = channels_.find(name);
        if (it == channels_.end() || !it->second->enabled()) {
            continue;  // 未注册 / 已禁用 -> 不计为失败
        }

        // 按用户、按渠道冷却（设计文档 13.6）。缺少 device_id 时无法去重，
        // 跳过冷却检查。
        if (!req.device_id.empty()) {
            const std::string key =
                cooldown::key(req.event_type, req.device_id, user, name);
            if (!cooldown_.tryAcquire(key, cooldownTtl)) {
                covered = true;  // 近期已通过此渠道送达
                continue;
            }
        }

        // 跨过冷却闸门即视为「已尝试该主渠道」：后续 no_target / resolve_error
        // 的 continue 也计入，保证全部主渠道都无 target 时兜底仍触发。
        anyPrimaryAttempted = true;

        ChannelPayload perUser = payload;
        perUser.recipients = {user};
        if (it->second->capabilities().requiresTarget) {
            const auto r =
                targetResolver_
                    ? targetResolver_->resolve(name, user)
                    : TargetResolution{std::nullopt, "no target resolver"};
            if (!r.error.empty()) {
                LOG_ERROR << "target resolve error channel=" << name
                          << " user=" << user << " err=" << r.error;
                attempts.push_back(
                    {name, false, false, "resolve_error", user, 0});
                continue;
            }
            if (!r.value.has_value()) {
                attempts.push_back({name, false, false, "no_target", user, 0});
                continue;
            }
            perUser.target = *r.value;
        }

        const ChannelResult rc = it->second->send(perUser);
        attempts.push_back(
            {name, rc.success, false, rc.message, user, rc.errcode});
        if (rc.success) {
            covered = true;
        }
    }

    if (covered) {
        return true;
    }

    // 兜底（设计文档 13.3）：仅当用户未被任何主渠道覆盖、且至少尝试过一个主
    // 渠道（或主渠道列表为空）时触发。
    if (!fallbackEnabled_) {
        return false;
    }
    if (!anyPrimaryAttempted && !rule.channels.empty()) {
        return false;
    }
    const auto fb = locateFallbackChannel();
    if (!fb) {
        return false;
    }

    if (!req.device_id.empty()) {
        const std::string key =
            cooldown::key(req.event_type, req.device_id, user, fb->name());
        if (!cooldown_.tryAcquire(key, cooldownTtl)) {
            return true;  // 该用户近期已就此事收到过兜底
        }
    }

    ChannelPayload fbPayload = payload;
    fbPayload.recipients = {user};
    if (fb->capabilities().requiresTarget) {
        const auto r =
            targetResolver_ ? targetResolver_->resolve(fb->name(), user)
                            : TargetResolution{std::nullopt, "no target resolver"};
        if (!r.error.empty()) {
            LOG_ERROR << "target resolve error channel=" << fb->name()
                      << " user=" << user << " err=" << r.error;
            attempts.push_back({fb->name(), false, true, "resolve_error", user, 0});
            return false;
        }
        if (!r.value.has_value()) {
            attempts.push_back({fb->name(), false, true, "no_target", user, 0});
            return false;
        }
        fbPayload.target = *r.value;
    }

    const ChannelResult rc = fb->send(fbPayload);
    attempts.push_back({fb->name(), rc.success, true, rc.message, user, rc.errcode});
    return rc.success;
}

RetryOutcome NotifyService::redispatch(const NotifySendRequest& req) {
    const PushRule* rule = findRule(req.event_type);
    if (rule == nullptr || (rule->condition && !rule->condition(req))) {
        return RetryOutcome{true, "", ""};
    }

    ChannelPayload payload = buildPayload(req, *rule);
    resolveRecipients(*rule, req, payload.recipients);
    if (payload.recipients.empty()) {
        return RetryOutcome{true, "", ""};
    }

    bool allCovered = true;
    std::string failedDetail;
    for (const auto& user : payload.recipients) {
        if (!redispatchToUser(*rule, req, payload, user, failedDetail)) {
            allCovered = false;
        }
    }

    return RetryOutcome{allCovered,
                        allCovered ? "" : "some recipients undelivered",
                        failedDetail};
}

bool NotifyService::redispatchToUser(const PushRule& rule,
                                     const NotifySendRequest& req,
                                     const ChannelPayload& payload,
                                     const std::string& user,
                                     std::string& failedDetail) {
    const std::string notify_id = http_util::newId("ntf");
    const bool dedup = !req.event_id.empty() && pendingRepo_;

    auto claimSlot = [&](const std::string& channel) {
        return dedup ? pendingRepo_->tryMarkDispatched(req.event_id, user,
                                                       channel, notify_id)
                     : DispatchClaim::Claimed;
    };
    auto releaseSlot = [&](const std::string& channel) {
        if (dedup) {
            pendingRepo_->clearDispatched(req.event_id, user, channel);
        }
    };

    bool covered = false;
    bool anyPrimaryAttempted = false;

    for (const auto& name : rule.channels) {
        const auto it = channels_.find(name);
        if (it == channels_.end() || !it->second->enabled()) {
            continue;
        }

        if (claimSlot(name) == DispatchClaim::AlreadyDone) {
            covered = true;  // 已通过此渠道送达
            continue;
        }

        anyPrimaryAttempted = true;

        ChannelPayload perUser = payload;
        perUser.recipients = {user};
        if (it->second->capabilities().requiresTarget) {
            const auto r =
                targetResolver_
                    ? targetResolver_->resolve(name, user)
                    : TargetResolution{std::nullopt, "no target resolver"};
            if (!r.error.empty() || !r.value.has_value()) {
                releaseSlot(name);  // 未发送，释放槽位
                continue;
            }
            perUser.target = *r.value;
        }

        const ChannelResult rc = it->second->send(perUser);
        if (rc.success) {
            covered = true;
        } else {
            releaseSlot(name);  // 释放槽位，以便后续重试可重新发送
        }
    }

    if (covered) {
        return true;
    }

    if (!fallbackEnabled_) {
        failedDetail += user;
        return false;
    }
    if (!anyPrimaryAttempted && !rule.channels.empty()) {
        failedDetail += user;
        return false;
    }
    const auto fb = locateFallbackChannel();
    if (!fb) {
        failedDetail += user;
        return false;
    }

    if (claimSlot(fb->name()) == DispatchClaim::AlreadyDone) {
        return true;
    }

    ChannelPayload fbPayload = payload;
    fbPayload.recipients = {user};
    if (fb->capabilities().requiresTarget) {
        const auto r =
            targetResolver_ ? targetResolver_->resolve(fb->name(), user)
                            : TargetResolution{std::nullopt, "no target resolver"};
        if (!r.error.empty() || !r.value.has_value()) {
            releaseSlot(fb->name());
            failedDetail += user;
            return false;
        }
        fbPayload.target = *r.value;
    }

    const ChannelResult rc = fb->send(fbPayload);
    if (rc.success) {
        return true;
    }
    releaseSlot(fb->name());
    failedDetail += user;
    return false;
}
