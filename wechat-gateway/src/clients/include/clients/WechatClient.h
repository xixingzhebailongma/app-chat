#pragma once

#include <functional>
#include <string>

// 微信 jscode2session 调用的结果（设计文档 六）。
struct WechatSession {
    bool ok = false;
    std::string openid;
    std::string session_key;
    std::string unionid;
    std::string errmsg;
};

// 公众号网页授权 code -> openid 调用的结果（设计文档 十一）。
struct WechatOauthResult {
    bool ok = false;
    std::string openid;
    std::string errmsg;
};

// 微信认证接口的轻量客户端：小程序的 `jscode2session`
// （设计文档 六）以及公众号网页授权的 `sns/oauth2/access_token`（十一）。
class WechatClient {
public:
    using SessionCallback = std::function<void(const WechatSession&)>;
    using OauthCallback = std::function<void(const WechatOauthResult&)>;

    WechatClient(std::string appid, std::string secret,
                 std::string oaAppid = "", std::string oaSecret = "");

    void jscode2session(const std::string& code, SessionCallback cb);

    // 用公众号网页授权 code 换取 OA openid（snsapi_base /
    // snsapi_userinfo）。用于 GET /api/oa/bind/authorize（设计文档 十一）。
    void oauth2AccessToken(const std::string& code, OauthCallback cb);

private:
    std::string appid_;
    std::string secret_;
    std::string oaAppid_;
    std::string oaSecret_;
};
