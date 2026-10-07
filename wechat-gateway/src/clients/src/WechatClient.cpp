#include "clients/WechatClient.h"

#include <drogon/HttpClient.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpTypes.h>
#include <nlohmann/json.hpp>

#include <utility>

WechatClient::WechatClient(std::string appid, std::string secret,
                           std::string oaAppid, std::string oaSecret)
    : appid_(std::move(appid)),
      secret_(std::move(secret)),
      oaAppid_(std::move(oaAppid)),
      oaSecret_(std::move(oaSecret)) {}

void WechatClient::jscode2session(const std::string& code,
                                  SessionCallback cb) {
    auto client = drogon::HttpClient::newHttpClient("https://api.weixin.qq.com");
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/sns/jscode2session");
    req->setParameter("appid", appid_);
    req->setParameter("secret", secret_);
    req->setParameter("js_code", code);
    req->setParameter("grant_type", "authorization_code");

    client->sendRequest(
        req, [cb = std::move(cb)](drogon::ReqResult result,
                                  const drogon::HttpResponsePtr& resp) {
            WechatSession s;
            if (result != drogon::ReqResult::Ok || !resp) {
                s.ok = false;
                s.errmsg = "wechat jscode2session request failed";
                cb(s);
                return;
            }

            auto body = nlohmann::json::parse(resp->getBody(), nullptr, false);
            if (body.is_discarded() || !body.is_object()) {
                s.ok = false;
                s.errmsg = "invalid wechat response";
                cb(s);
                return;
            }
            if (body.contains("errcode") &&
                body["errcode"].get<int>() != 0) {
                s.ok = false;
                s.errmsg = body.value("errmsg", "wechat error");
                cb(s);
                return;
            }
            if (!body.contains("openid") || !body["openid"].is_string()) {
                s.ok = false;
                s.errmsg = "no openid in wechat response";
                cb(s);
                return;
            }

            s.ok = true;
            s.openid = body["openid"].get<std::string>();
            s.session_key = body.value("session_key", "");
            s.unionid = body.value("unionid", "");
            cb(s);
        });
}

void WechatClient::oauth2AccessToken(const std::string& code,
                                     OauthCallback cb) {
    auto client = drogon::HttpClient::newHttpClient("https://api.weixin.qq.com");
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/sns/oauth2/access_token");
    req->setParameter("appid", oaAppid_);
    req->setParameter("secret", oaSecret_);
    req->setParameter("code", code);
    req->setParameter("grant_type", "authorization_code");

    client->sendRequest(
        req, [cb = std::move(cb)](drogon::ReqResult result,
                                  const drogon::HttpResponsePtr& resp) {
            WechatOauthResult r;
            if (result != drogon::ReqResult::Ok || !resp) {
                r.ok = false;
                r.errmsg = "wechat oauth2 request failed";
                cb(r);
                return;
            }

            auto body = nlohmann::json::parse(resp->getBody(), nullptr, false);
            if (body.is_discarded() || !body.is_object()) {
                r.ok = false;
                r.errmsg = "invalid wechat response";
                cb(r);
                return;
            }
            if (body.contains("errcode") &&
                body["errcode"].get<int>() != 0) {
                r.ok = false;
                r.errmsg = body.value("errmsg", "wechat error");
                cb(r);
                return;
            }
            if (!body.contains("openid") || !body["openid"].is_string()) {
                r.ok = false;
                r.errmsg = "no openid in wechat response";
                cb(r);
                return;
            }

            r.ok = true;
            r.openid = body["openid"].get<std::string>();
            cb(r);
        });
}
