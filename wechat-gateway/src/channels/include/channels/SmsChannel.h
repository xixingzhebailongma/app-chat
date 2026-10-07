#pragma once

#include <map>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include "channels/INotifyChannel.h"
#include "clients/SmsProvider.h"

// 单个 event_type 的短信模板配置：模板 code + 占位符参数。
// `params` 为对象（变量名 -> 占位符）或数组（位置参数）。
struct SmsTemplateConfig {
    std::string code;
    nlohmann::json params;
};

class SmsChannel : public INotifyChannel {
public:
    explicit SmsChannel(bool enabled = true,
                        std::shared_ptr<SmsProvider> provider = nullptr,
                        bool realMode = false, std::string signName = "",
                        std::map<std::string, SmsTemplateConfig> templates = {});

    std::string name() const override { return "sms"; }
    bool enabled() const override { return enabled_; }
    ChannelCapabilities capabilities() const override {
        ChannelCapabilities c;
        c.requiresTarget = true;
        c.isFallback = true;
        return c;
    }
    ChannelResult send(const ChannelPayload& payload) override;

private:
    bool enabled_;
    std::shared_ptr<SmsProvider> provider_;
    bool realMode_;
    std::string signName_;
    std::map<std::string, SmsTemplateConfig> templates_;
};
