#include "channels/WechatOaChannel.h"

#include <drogon/HttpClient.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpTypes.h>
#include <nlohmann/json.hpp>
#include <trantor/utils/Logger.h>

#include <string>
#include <utility>

namespace {

bool isTokenStaleErrcode(int errcode) {
    return errcode == 40001 || errcode == 40014 || errcode == 42001;
}

// 公众号模板消息单次发送。
struct OaSendOutcome {
    bool network_error = false;
    int errcode = 0;
    std::string errmsg;
};

OaSendOutcome postOaTemplateSend(const std::string& apiBaseUrl,
                                 const std::string& accessToken,
                                 const OaPayload& p) {
    OaSendOutcome out;

    nlohmann::json body;
    body["touser"] = p.oa_openid;
    body["template_id"] = p.template_id;
    nlohmann::json data = nlohmann::json::object();
    data["thing1"] = nlohmann::json{{"value", p.student_name}};
    data["thing2"] = nlohmann::json{{"value", p.space_name}};
    data["time3"] = nlohmann::json{{"value", p.arrival_time}};
    body["data"] = data;

    auto client = drogon::HttpClient::newHttpClient(apiBaseUrl);
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath("/cgi-bin/message/template/send");
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

WechatOaChannel::WechatOaChannel(std::shared_ptr<WechatTokenManager> tokens,
                                 std::string apiBaseUrl,
                                 std::string arrivalTemplateId, bool enabled,
                                 bool realMode)
    : enabled_(enabled),
      realMode_(realMode),
      tokens_(std::move(tokens)),
      apiBaseUrl_(std::move(apiBaseUrl)),
      arrivalTemplateId_(std::move(arrivalTemplateId)) {}

ChannelResult WechatOaChannel::send(const OaPayload& payload) {
    if (payload.oa_openid.empty()) {
        return ChannelResult{name(), false, "no_oa_openid"};
    }

    // mock 开关：显式 mode=mock 时才 dry-run（启动日志 + 每次发送打日志），
    // 与其它渠道一致，绝不静默假装成功。
    if (!realMode_) {
        LOG_INFO << "wechat_oa (dry-run): student_no=" << payload.student_no
                 << " space=" << payload.space_name;
        return ChannelResult{name(), true, "mock: wechat_oa (dry-run)"};
    }

    // real 模式但模板仍是占位符（tmpl_* 或空）：属配置错误，返回明确
    // 失败原因（not_configured），由 OaNotifyService 映射为 503，绝不静默 mock。
    if (!isConfigured()) {
        LOG_WARN << "wechat_oa real mode but arrival template not configured "
                    "(empty or tmpl_* placeholder); refusing to send";
        return ChannelResult{name(), false, "not_configured", 0,
                             "arrival template not configured"};
    }
    if (!tokens_) {
        return ChannelResult{name(), false, "no token manager"};
    }

    std::string token = tokens_->get(ChannelType::Oa);
    if (token.empty()) {
        return ChannelResult{name(), false, "failed to obtain access_token"};
    }

    OaPayload p = payload;
    p.template_id = arrivalTemplateId_;

    OaSendOutcome outcome = postOaTemplateSend(apiBaseUrl_, token, p);
    if (!outcome.network_error && isTokenStaleErrcode(outcome.errcode)) {
        tokens_->forceRefresh(ChannelType::Oa);
        token = tokens_->get(ChannelType::Oa);
        if (!token.empty()) {
            outcome = postOaTemplateSend(apiBaseUrl_, token, p);
        }
    }

    if (outcome.network_error) {
        LOG_WARN << "wechat_oa template send network error";
        return ChannelResult{name(), false, "wechat request failed", 0,
                             "network error"};
    }
    if (outcome.errcode == 0) {
        return ChannelResult{name(), true, "ok", 0, "ok"};
    }
    LOG_WARN << "wechat_oa template send failed: errcode=" << outcome.errcode
             << " errmsg=" << outcome.errmsg;
    return ChannelResult{name(), false,
                         "wechat errcode " + std::to_string(outcome.errcode),
                         outcome.errcode, outcome.errmsg};
}
