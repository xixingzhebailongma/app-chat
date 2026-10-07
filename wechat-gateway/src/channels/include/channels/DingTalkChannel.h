#pragma once

#include <functional>
#include <memory>
#include <string>

#include "channels/INotifyChannel.h"

class RedisClient;
class AppTokenCache;

// 钉钉工作通知单次发送结果。
struct DingTalkOutcome {
    bool network_error = false;
    int errcode = 0;
    std::string errmsg;
};

class DingTalkChannel : public INotifyChannel {
public:
    using Transporter = std::function<DingTalkOutcome(
        const std::string& accessToken, const std::string& userId,
        const std::string& content)>;

    explicit DingTalkChannel(
        bool enabled = true, bool realMode = false, std::string appKey = "",
        std::string appSecret = "", std::string agentId = "",
        std::string apiBaseUrl = "https://oapi.dingtalk.com",
        std::shared_ptr<RedisClient> redis = nullptr,
        Transporter transporter = nullptr);

    std::string name() const override { return "dingtalk"; }
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
    DingTalkOutcome sendOnce(const std::string& token, const std::string& userId,
                             const std::string& content);

    bool enabled_;
    bool realMode_;
    std::string appKey_;
    std::string appSecret_;
    std::string agentId_;
    std::string apiBaseUrl_;
    Transporter transporter_;
    std::shared_ptr<AppTokenCache> tokenCache_;
};
