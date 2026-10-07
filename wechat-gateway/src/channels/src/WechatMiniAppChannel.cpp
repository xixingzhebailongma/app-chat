#include "channels/WechatMiniAppChannel.h"

#include <drogon/HttpClient.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpTypes.h>
#include <nlohmann/json.hpp>
#include <trantor/utils/Logger.h>

#include <string>
#include <utility>

#include "channels/ChannelFactory.h"
#include "db/SubscriptionRepository.h"
#include "utils/TemplateRender.h"

namespace {

// 组装订阅消息请求体并发送一次，返回微信响应的 errcode/errmsg。
// 照 WechatTokenManager::fetchFromWechat 的同步 HttpClient 写法
// （因此绝不能在 Drogon 事件循环线程上调用）。
SubscribeSendOutcome postSubscribeSend(const std::string& apiBaseUrl,
                                       const std::string& accessToken,
                                       const SubscribeSendArgs& args) {
    SubscribeSendOutcome out;

    nlohmann::json body;
    body["touser"] = args.openid;
    body["template_id"] = args.template_id;
    if (!args.page.empty()) {
        body["page"] = args.page;
    }
    body["miniprogram_state"] =
        args.miniprogram_state.empty() ? "formal" : args.miniprogram_state;
    nlohmann::json data;
    for (const auto& [key, value] : args.data) {
        data[key] = nlohmann::json{{"value", value}};
    }
    body["data"] = data;

    auto client = drogon::HttpClient::newHttpClient(apiBaseUrl);
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath("/cgi-bin/message/subscribe/send");
    req->setParameter("access_token", accessToken);
    req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    req->setBody(body.dump());

    const auto [result, resp] = client->sendRequest(req);
    if (result != drogon::ReqResult::Ok || !resp) {
        out.network_error = true;
        return out;
    }

    const auto respBody =
        nlohmann::json::parse(resp->getBody(), nullptr, false);
    if (respBody.is_discarded() || !respBody.is_object()) {
        out.network_error = true;
        return out;
    }
    out.errcode = respBody.value("errcode", 0);
    out.errmsg = respBody.value("errmsg", std::string("ok"));
    return out;
}

}  // namespace

WechatMiniAppChannel::WechatMiniAppChannel(
    bool enabled, std::shared_ptr<WechatTokenManager> tokens,
    std::string apiBaseUrl, Transporter transporter, bool realMode,
    std::shared_ptr<SubscriptionRepository> subscriptionRepo)
    : enabled_(enabled),
      tokens_(std::move(tokens)),
      apiBaseUrl_(std::move(apiBaseUrl)),
      transporter_(std::move(transporter)),
      realMode_(realMode),
      subscriptionRepo_(std::move(subscriptionRepo)) {}

bool WechatMiniAppChannel::isTokenStaleErrcode(int errcode) {
    return errcode == 40001 || errcode == 40014 || errcode == 42001;
}

bool WechatMiniAppChannel::isUserRefusedErrcode(int errcode) {
    return errcode == 43101;
}

void WechatMiniAppChannel::setTemplates(
    std::map<std::string, std::string> templateIds,
    std::map<std::string, std::map<std::string, std::string>> templateFields) {
    templateIds_ = std::move(templateIds);
    templateFields_ = std::move(templateFields);
}

SubscribeSendOutcome WechatMiniAppChannel::sendOnce(
    const std::string& accessToken, const SubscribeSendArgs& args) {
    if (transporter_) {
        return transporter_(accessToken, args);
    }
    return postSubscribeSend(apiBaseUrl_, accessToken, args);
}

ChannelResult WechatMiniAppChannel::sendWithArgs(const SubscribeSendArgs& args) {
    // mock 开关：演示环境绝不触达真实微信 API。
    if (!realMode_) {
        LOG_INFO << "wechat_miniapp (dry-run): template=" << args.template_id
                 << " openid=" << args.openid << " page=" << args.page;
        return ChannelResult{name(), true, "mock: miniapp (dry-run)"};
    }

    if (args.openid.empty()) {
        return ChannelResult{name(), false, "no miniapp openid (touser)", 40003,
                             "invalid touser"};
    }
    if (args.template_id.empty()) {
        return ChannelResult{name(), false, "no template_id", 0,
                             "missing template_id"};
    }
    if (!tokens_) {
        return ChannelResult{name(), false, "no token manager", 0,
                             "token manager not configured"};
    }

    std::string token = tokens_->get(ChannelType::Miniapp);
    if (token.empty()) {
        return ChannelResult{name(), false, "failed to obtain access_token", 0,
                             "no access_token"};
    }

    SubscribeSendOutcome outcome = sendOnce(token, args);

    // 40001/40014/42001：令牌过期/失效 → 强制刷新后重试一次。
    if (!outcome.network_error && isTokenStaleErrcode(outcome.errcode)) {
        tokens_->forceRefresh(ChannelType::Miniapp);
        token = tokens_->get(ChannelType::Miniapp);
        if (!token.empty()) {
            outcome = sendOnce(token, args);
        }
    }

    if (outcome.network_error) {
        LOG_WARN << "wechat_miniapp subscribe send network error (openid="
                 << args.openid << ")";
        return ChannelResult{name(), false, "wechat request failed", 0,
                             "network error"};
    }
    if (outcome.errcode == 0) {
        return ChannelResult{name(), true, "ok", 0, "ok"};
    }

    LOG_WARN << "wechat_miniapp subscribe send failed: errcode="
             << outcome.errcode << " errmsg=" << outcome.errmsg
             << " (template=" << args.template_id << ")";
    return ChannelResult{name(), false,
                         "wechat errcode " + std::to_string(outcome.errcode),
                         outcome.errcode, outcome.errmsg,
                         isUserRefusedErrcode(outcome.errcode)};
}

ChannelResult WechatMiniAppChannel::send(const ChannelPayload& payload) {
    const std::string user =
        payload.recipients.empty() ? std::string() : payload.recipients.front();

    // 1. 读 target（NotifyService 已解析 openid）。空 -> 不触碰额度。
    if (payload.target.empty()) {
        return ChannelResult{name(), false, "no_target", 0,
                             "no miniapp openid"};
    }

    // 2. 组装 template_id / data / page（渠道自持模板，不再由 NotifyService 组装）。
    const auto tidIt = templateIds_.find(payload.event_type);
    if (tidIt == templateIds_.end() || tidIt->second.empty()) {
        return ChannelResult{name(), false, "no template_id", 0,
                             "missing template_id for event"};
    }
    const std::string templateId = tidIt->second;

    SubscribeSendArgs args;
    args.openid = payload.target;
    args.template_id = templateId;
    args.page = payload.event_id.empty()
                    ? "/pages/alerts/alerts"
                    : "/pages/alerts/detail?id=" + payload.event_id;
    args.miniprogram_state = "formal";

    const auto fieldsIt = templateFields_.find(payload.event_type);
    if (fieldsIt != templateFields_.end()) {
        const auto vars = tmpl::notifyVars(payload.content, payload.device_label,
                                           payload.space_id, payload.severity);
        for (const auto& [key, tpl] : fieldsIt->second) {
            args.data[key] = tmpl::render(tpl, vars);
        }
    }

    // 3. 订阅额度：发送前原子预扣。无额度 -> 跳过该渠道（触发兜底）。
    bool reserved = false;
    if (subscriptionRepo_ && !user.empty()) {
        if (!subscriptionRepo_->reserve(user, templateId)) {
            LOG_INFO << "skip wechat_miniapp for user " << user
                     << " (no quota for template " << templateId << ")";
            return ChannelResult{name(), false, "no_quota", 0, "no quota"};
        }
        reserved = true;
    }

    // 4. 发送。
    ChannelResult r = sendWithArgs(args);

    // 5. 额度回补 / 清零。
    if (reserved && subscriptionRepo_ && !user.empty()) {
        if (r.unsubscribed) {
            subscriptionRepo_->revoke(user, templateId);
            LOG_INFO << "wechat_miniapp 43101 for user " << user << " template "
                     << templateId << "; revoking that template";
        } else if (!r.success) {
            subscriptionRepo_->refill(user, templateId);
        }
    }
    return r;
}

ChannelResult WechatMiniAppChannel::sendDirect(
    const std::string& openid, const std::string& templateId,
    const std::string& page, const std::string& miniprogramState,
    const std::map<std::string, std::string>& data) {
    SubscribeSendArgs args;
    args.openid = openid;
    args.template_id = templateId;
    args.page = page;
    args.miniprogram_state = miniprogramState;
    args.data = data;
    return sendWithArgs(args);
}

namespace {

// 小程序订阅消息渠道工厂注册（7.9 插件化）。builder 只捕获 deps、不在构造期
// 解引用；模板 ID / data 字段的解析仍在 main（env 覆盖 + prod 校验），经
// setTemplates 注入。
const bool kMiniappRegistered = [] {
    ChannelFactory::instance().registerBuilder(
        "wechat_miniapp", [](const ChannelConfig& c, const ChannelDeps& deps) {
            const bool realMode = channelCfgGet(c.raw, "mode", "mock") == "real";
            return std::make_shared<WechatMiniAppChannel>(
                c.enabled, deps.tokens, deps.wechatApiBaseUrl, nullptr,
                realMode, deps.subscriptionRepo);
        });
    return true;
}();

}  // namespace
