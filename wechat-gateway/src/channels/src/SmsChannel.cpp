#include "channels/SmsChannel.h"

#include <nlohmann/json.hpp>
#include <trantor/utils/Logger.h>

#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

#include "channels/ChannelFactory.h"
#include "clients/SmsProviderFactory.h"
#include "utils/TemplateRender.h"

SmsChannel::SmsChannel(bool enabled, std::shared_ptr<SmsProvider> provider,
                       bool realMode, std::string signName,
                       std::map<std::string, SmsTemplateConfig> templates)
    : enabled_(enabled),
      provider_(std::move(provider)),
      realMode_(realMode),
      signName_(std::move(signName)),
      templates_(std::move(templates)) {}

ChannelResult SmsChannel::send(const ChannelPayload& payload) {
    const std::string user =
        payload.recipients.empty() ? "" : payload.recipients.front();

    // mock 开关：演示环境绝不真发短信，也不查手机号。
    if (!realMode_) {
        LOG_INFO << "sms (dry-run): event=" << payload.event_type
                 << " user=" << user << " device=" << payload.device_id
                 << " content=" << payload.content;
        return ChannelResult{name(), true, "mock: sms sent (dry-run)"};
    }

    if (!provider_) {
        return ChannelResult{name(), false, "no sms provider configured", 0,
                             "provider not configured"};
    }

    // 手机号由 NotifyService 统一解析（resolver 含 go-backend 回落）写入 target。
    const std::string phone = payload.target;
    if (phone.empty()) {
        return ChannelResult{name(), false, "no phone for user", 0,
                             "user has no phone"};
    }

    const auto it = templates_.find(payload.event_type);
    if (it == templates_.end() || it->second.code.empty()) {
        return ChannelResult{name(), false, "no sms template for event", 0,
                             "missing template"};
    }

    const auto vars =
        tmpl::notifyVars(payload.content, payload.device_label, payload.space_id,
                         payload.severity);
    nlohmann::json rendered = nlohmann::json::object();
    if (it->second.params.is_array()) {
        rendered = nlohmann::json::array();
        for (const auto& p : it->second.params) {
            rendered.push_back(tmpl::render(p.get<std::string>(), vars));
        }
    } else if (it->second.params.is_object()) {
        for (auto pit = it->second.params.begin();
             pit != it->second.params.end(); ++pit) {
            rendered[pit.key()] =
                tmpl::render(pit.value().get<std::string>(), vars);
        }
    }

    const SmsSendResult r =
        provider_->send(phone, signName_, it->second.code, rendered);
    if (r.ok) {
        return ChannelResult{name(), true, "ok"};
    }
    return ChannelResult{name(), false, r.errmsg, r.errcode, r.errmsg};
}

namespace {

// SMS 渠道工厂注册（7.9 插件化）。builder 只捕获 deps、不在构造期解引用，
// 这样工厂测试可传空 deps 只验类型。
const bool kSmsRegistered = [] {
    ChannelFactory::instance().registerBuilder(
        "sms", [](const ChannelConfig& c, const ChannelDeps&) {
            const bool realMode = channelCfgGet(c.raw, "mode", "mock") == "real";
            const std::string providerName =
                channelCfgGet(c.raw, "provider", "aliyun");
            std::string accessKey = channelCfgGet(c.raw, "access_key", "");
            std::string secret = channelCfgGet(c.raw, "secret", "");
            const std::string region =
                channelCfgGet(c.raw, "region", "cn-hangzhou");
            const std::string sdkAppId = channelCfgGet(c.raw, "sdk_app_id", "");
            const std::string signName = channelCfgGet(c.raw, "sign_name", "");

            std::map<std::string, SmsTemplateConfig> templates;
            if (c.raw.is_object() && c.raw.contains("templates")) {
                const auto& tpls = c.raw["templates"];
                if (tpls.is_object()) {
                    for (auto it = tpls.begin(); it != tpls.end(); ++it) {
                        SmsTemplateConfig tc;
                        const auto& t = it.value();
                        if (t.is_object()) {
                            tc.code = t.value("code", "");
                            if (t.contains("params")) {
                                tc.params = t["params"];
                            }
                        }
                        templates[it.key()] = std::move(tc);
                    }
                }
            }

            // 与 OA 家长绑定验证码短信共用同一 provider 工厂（含密钥 env 覆盖）。
            std::shared_ptr<SmsProvider> provider = sms::makeProvider(
                providerName, accessKey, secret, region, sdkAppId);

            return std::make_shared<SmsChannel>(c.enabled, provider, realMode,
                                                signName, templates);
        });
    return true;
}();

}  // namespace
