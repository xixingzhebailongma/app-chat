#include "channels/WeComChannel.h"

#include <drogon/HttpClient.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpTypes.h>
#include <nlohmann/json.hpp>
#include <trantor/utils/Logger.h>

#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

#include "channels/ChannelFactory.h"
#include "clients/AppTokenCache.h"
#include "db/RedisClient.h"

namespace {

WeComOutcome postWeComSend(const std::string& apiBaseUrl,
                           const std::string& accessToken,
                           const std::string& agentId,
                           const std::string& userId,
                           const std::string& content) {
    WeComOutcome out;

    nlohmann::json body = {
        {"touser", userId},
        {"msgtype", "text"},
        {"agentid", agentId},
        {"text", {{"content", content}}},
    };

    auto client = drogon::HttpClient::newHttpClient(apiBaseUrl);
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath("/cgi-bin/message/send");
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

WeComChannel::WeComChannel(bool enabled, bool realMode, std::string corpId,
                           std::string corpSecret, std::string agentId,
                           std::string apiBaseUrl,
                           std::shared_ptr<RedisClient> redis,
                           Transporter transporter)
    : enabled_(enabled),
      realMode_(realMode),
      corpId_(std::move(corpId)),
      corpSecret_(std::move(corpSecret)),
      agentId_(std::move(agentId)),
      apiBaseUrl_(std::move(apiBaseUrl)),
      transporter_(std::move(transporter)) {
    // token 缓存/锁 key 带租户前缀（corp_id），多学校部署防互踩。
    const std::string key = "wecom:token:" + corpId_;
    tokenCache_ = std::make_shared<AppTokenCache>(
        redis, key, key + ":lock", [this]() -> std::string {
            auto client = drogon::HttpClient::newHttpClient(apiBaseUrl_);
            auto req = drogon::HttpRequest::newHttpRequest();
            req->setMethod(drogon::Get);
            req->setPath("/cgi-bin/gettoken");
            req->setParameter("corpid", corpId_);
            req->setParameter("corpsecret", corpSecret_);
            const auto [result, resp] = client->sendRequest(req);
            if (result != drogon::ReqResult::Ok || !resp) {
                return "";
            }
            const auto body =
                nlohmann::json::parse(resp->getBody(), nullptr, false);
            if (body.is_discarded() || !body.is_object()) {
                return "";
            }
            if (body.contains("access_token") &&
                body["access_token"].is_string()) {
                return body["access_token"].get<std::string>();
            }
            return "";
        });
}

bool WeComChannel::isTokenStaleErrcode(int errcode) {
    return errcode == 40001 || errcode == 40014 || errcode == 42001;
}

std::string WeComChannel::getToken() {
    return tokenCache_ ? tokenCache_->get() : std::string();
}

WeComOutcome WeComChannel::sendOnce(const std::string& token,
                                    const std::string& userId,
                                    const std::string& content) {
    if (transporter_) {
        return transporter_(token, userId, content);
    }
    return postWeComSend(apiBaseUrl_, token, agentId_, userId, content);
}

ChannelResult WeComChannel::send(const ChannelPayload& payload) {
    const std::string user =
        payload.recipients.empty() ? "" : payload.recipients.front();

    if (!realMode_) {
        LOG_INFO << "wecom (dry-run): event=" << payload.event_type
                 << " user=" << user << " content=" << payload.content;
        return ChannelResult{name(), true, "mock: wecom (dry-run)"};
    }
    if (payload.target.empty()) {
        return ChannelResult{name(), false, "no_target", 0, "no wecom userid"};
    }
    if (agentId_.empty()) {
        return ChannelResult{name(), false, "no agent_id", 0,
                             "agent_id not configured"};
    }

    std::string token = getToken();
    if (token.empty()) {
        return ChannelResult{name(), false, "failed to obtain access_token", 0,
                             "no access_token"};
    }

    WeComOutcome outcome = sendOnce(token, payload.target, payload.content);
    if (!outcome.network_error && isTokenStaleErrcode(outcome.errcode)) {
        if (tokenCache_) {
            tokenCache_->forceRefresh();
        }
        token = getToken();
        if (!token.empty()) {
            outcome = sendOnce(token, payload.target, payload.content);
        }
    }

    if (outcome.network_error) {
        LOG_WARN << "wecom send network error (user=" << payload.target << ")";
        return ChannelResult{name(), false, "wecom request failed", 0,
                             "network error"};
    }
    if (outcome.errcode == 0) {
        return ChannelResult{name(), true, "ok", 0, "ok"};
    }
    LOG_WARN << "wecom send failed: errcode=" << outcome.errcode
             << " errmsg=" << outcome.errmsg;
    return ChannelResult{name(), false,
                         "wecom errcode " + std::to_string(outcome.errcode),
                         outcome.errcode, outcome.errmsg};
}

namespace {

// 企业微信应用消息渠道工厂注册（7.9 插件化）。
const bool kWeComRegistered = [] {
    ChannelFactory::instance().registerBuilder(
        "wecom", [](const ChannelConfig& c, const ChannelDeps& deps) {
            const bool realMode = channelCfgGet(c.raw, "mode", "mock") == "real";
            const std::string corpId = channelCfgGet(c.raw, "corp_id", "");
            std::string corpSecret = channelCfgGet(c.raw, "corp_secret", "");
            const std::string agentId = channelCfgGet(c.raw, "agent_id", "");
            const std::string apiBase = channelCfgGet(
                c.raw, "api_base_url", "https://qyapi.weixin.qq.com");
            if (const char* e = std::getenv("WECOM_CORP_SECRET"); e && *e) {
                corpSecret = e;
            }
            return std::make_shared<WeComChannel>(
                c.enabled, realMode, corpId, corpSecret, agentId, apiBase,
                deps.redis);
        });
    return true;
}();

}  // namespace
