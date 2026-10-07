#pragma once

#include <functional>
#include <memory>
#include <string>

#include "channels/INotifyChannel.h"

class RedisClient;
class AppTokenCache;

// 企业微信应用消息单次发送结果。
struct WeComOutcome {
    bool network_error = false;
    int errcode = 0;
    std::string errmsg;
};

class WeComChannel : public INotifyChannel {
public:
    using Transporter = std::function<WeComOutcome(
        const std::string& accessToken, const std::string& userId,
        const std::string& content)>;

    explicit WeComChannel(
        bool enabled = true, bool realMode = false, std::string corpId = "",
        std::string corpSecret = "", std::string agentId = "",
        std::string apiBaseUrl = "https://qyapi.weixin.qq.com",
        std::shared_ptr<RedisClient> redis = nullptr,
        Transporter transporter = nullptr);

    std::string name() const override { return "wecom"; }
    bool enabled() const override { return enabled_; }
    ChannelCapabilities capabilities() const override {
        ChannelCapabilities c;
        c.requiresTarget = true;
        return c;
    }
    ChannelResult send(const ChannelPayload& payload) override;

    static bool isTokenStaleErrcode(int errcode);

private:
    std::string getToken();
    WeComOutcome sendOnce(const std::string& token, const std::string& userId,
                          const std::string& content);

    bool enabled_;
    bool realMode_;
    std::string corpId_;
    std::string corpSecret_;
    std::string agentId_;
    std::string apiBaseUrl_;
    Transporter transporter_;
    std::shared_ptr<AppTokenCache> tokenCache_;
};
