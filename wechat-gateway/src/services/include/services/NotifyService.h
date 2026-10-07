#pragma once

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "channels/INotifyChannel.h"
#include "db/NotifyLogRepository.h"
#include "db/NotifyTargetRepository.h"
#include "db/NotifyTargetResolver.h"
#include "db/PendingEventRepository.h"
#include "dto/NotifySendDto.h"
#include "services/PushRule.h"
#include "utils/Cooldown.h"

class AlertRepository;
class RedisClient;

struct RetryOutcome;  // 来自 services/RetryWorker.h（重新分派结果）

class NotifyService {
public:
    NotifyService(std::shared_ptr<NotifyLogRepository> logRepo,
                  std::shared_ptr<NotifyTargetRepository> targetRepo,
                  std::shared_ptr<PendingEventRepository> pendingRepo,
                  std::shared_ptr<RedisClient> redis = nullptr);

    void registerChannel(std::shared_ptr<INotifyChannel> channel);

    void setRules(const std::vector<PushRule>& rules) { rules_ = rules; }
    void setFallbackEnabled(bool enabled) { fallbackEnabled_ = enabled; }
    // 兜底渠道名（默认 "sms"）；空串 -> 按 capabilities().isFallback 自动发现。
    void setFallbackChannel(const std::string& channel) { fallbackChannel_ = channel; }
    void setDefaultCooldown(int seconds) { defaultCooldownSeconds_ = seconds; }

    // 配置驱动路由（7.9）：按 event_type 覆盖 rule.channels。校验并剔除
    // 未注册 / disabled 的渠道；列表可为空 -> 发送时直接走兜底。
    // 须在所有 registerChannel 之后调用（依赖 channels_ 已注册完成）。
    void setRouting(const std::vector<std::string>& defaultChannels,
                    const std::map<std::string, std::vector<std::string>>&
                        byEvent);

    // 渠道地址解析：requiresTarget 的渠道由本服务统一解析并填 ChannelPayload::target。
    void setTargetResolver(std::shared_ptr<INotifyTargetResolver> resolver) {
        targetResolver_ = std::move(resolver);
    }

    // 告警仓库：pushable 事件在分派前落库（设计文档 7.5②），失败不阻断推送。
    void setAlertRepo(std::shared_ptr<AlertRepository> repo) {
        alertRepo_ = std::move(repo);
    }
    // 告警落库失败计数（仅单测/调试断言用，非运行时指标）。
    uint64_t alertUpsertFailures() const { return alertUpsertFailures_.load(); }

    NotifySendResponse send(const NotifySendRequest& req);

    // 为重试工作线程重新分派挂起事件（设计文档 13.4）：
    // 按用户分派，以幂等台账而非冷却作为闸门。
    RetryOutcome redispatch(const NotifySendRequest& req);

private:
    const PushRule* findRule(const std::string& event_type) const;

    void resolveRecipients(const PushRule& rule,
                           const NotifySendRequest& req,
                           std::vector<std::string>& out) const;

    ChannelPayload buildPayload(const NotifySendRequest& req,
                                const PushRule& rule) const;

    bool dispatchToUser(const PushRule& rule,
                        const NotifySendRequest& req,
                        const ChannelPayload& payload,
                        const std::string& user,
                        int cooldownTtl,
                        std::vector<ChannelAttempt>& attempts);

    bool redispatchToUser(const PushRule& rule,
                          const NotifySendRequest& req,
                          const ChannelPayload& payload,
                          const std::string& user,
                          std::string& failedDetail);

    // 定位兜底渠道：fallbackChannel_ 显式指定优先，否则按 isFallback 扫描。
    std::shared_ptr<INotifyChannel> locateFallbackChannel() const;

    std::map<std::string, std::shared_ptr<INotifyChannel>> channels_;
    std::shared_ptr<NotifyLogRepository> logRepo_;
    std::shared_ptr<NotifyTargetRepository> targetRepo_;
    std::shared_ptr<PendingEventRepository> pendingRepo_;
    std::shared_ptr<INotifyTargetResolver> targetResolver_;
    std::shared_ptr<AlertRepository> alertRepo_;
    std::atomic<uint64_t> alertUpsertFailures_{0};
    std::vector<PushRule> rules_;
    bool fallbackEnabled_ = true;
    std::string fallbackChannel_ = "sms";
    int defaultCooldownSeconds_ = 1800;
    CooldownChecker cooldown_;
};
