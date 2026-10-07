#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "channels/INotifyChannel.h"
#include "clients/WechatTokenManager.h"

class SubscriptionRepository;

// 订阅消息单次 HTTP 发送的最终参数（模板组装后），供 transporter / sendOnce 使用。
struct SubscribeSendArgs {
    std::string openid;
    std::string template_id;
    std::string page;
    std::string miniprogram_state = "formal";
    std::map<std::string, std::string> data;
};

// 订阅消息单次 HTTP 发送的结果，供 sendWithArgs 分类处理（设计文档 十 / 13.8）。
struct SubscribeSendOutcome {
    bool network_error = false;
    int errcode = 0;
    std::string errmsg;
};

class WechatMiniAppChannel : public INotifyChannel {
public:
    // 可注入的「发送一次」函数（默认走真实微信订阅消息 API）。
    using Transporter = std::function<SubscribeSendOutcome(
        const std::string& accessToken, const SubscribeSendArgs& args)>;

    explicit WechatMiniAppChannel(
        bool enabled = true,
        std::shared_ptr<WechatTokenManager> tokens = nullptr,
        std::string apiBaseUrl = "https://api.weixin.qq.com",
        Transporter transporter = nullptr,
        bool realMode = true,
        std::shared_ptr<SubscriptionRepository> subscriptionRepo = nullptr);

    std::string name() const override { return "wechat_miniapp"; }
    bool enabled() const override { return enabled_; }
    ChannelCapabilities capabilities() const override {
        ChannelCapabilities c;
        c.requiresTarget = true;
        c.consumesQuota = true;
        return c;
    }
    ChannelResult send(const ChannelPayload& payload) override;

    // 最小触发接口直发（/internal/notify/subscribe-send）：不经过 NotifyService、
    // 不消耗订阅额度（调用方直接给出 openid/template_id/data）。
    ChannelResult sendDirect(const std::string& openid,
                             const std::string& templateId,
                             const std::string& page,
                             const std::string& miniprogramState,
                             const std::map<std::string, std::string>& data);

    // 模板配置注入（main 解析 + env 覆盖 + prod 校验后注入，替代旧
    // NotifyService::setMiniappTemplates）：event_type -> template_id / data 占位符。
    void setTemplates(
        std::map<std::string, std::string> templateIds,
        std::map<std::string, std::map<std::string, std::string>> templateFields);

    // 发送级 errcode 是否为 access_token 失效/过期，需 forceRefresh 后重试一次。
    static bool isTokenStaleErrcode(int errcode);

    // 发送级 errcode 是否为「用户拒收/退订」（43101）。
    static bool isUserRefusedErrcode(int errcode);

private:
    SubscribeSendOutcome sendOnce(const std::string& accessToken,
                                  const SubscribeSendArgs& args);

    // 发送核心：mock 开关 + 入参校验 + token 获取 + 40001 刷新重试 + errcode 分类。
    // 不触碰订阅额度（额度生命周期由 send() 负责）。
    ChannelResult sendWithArgs(const SubscribeSendArgs& args);

    bool enabled_;
    std::shared_ptr<WechatTokenManager> tokens_;
    std::string apiBaseUrl_;
    Transporter transporter_;
    bool realMode_;
    std::shared_ptr<SubscriptionRepository> subscriptionRepo_;
    std::map<std::string, std::string> templateIds_;       // event_type -> template_id
    std::map<std::string, std::map<std::string, std::string>> templateFields_;  // event_type -> (key -> tpl)
};
