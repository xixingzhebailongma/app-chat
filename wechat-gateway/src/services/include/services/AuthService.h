#pragma once

#include <drogon/HttpTypes.h>
#include <nlohmann/json.hpp>

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "clients/GoBackendClient.h"
#include "clients/WechatClient.h"
#include "db/WechatBindingRepository.h"
#include "utils/ErrorBody.h"

// 返回给控制器的结果：一个 HTTP 状态码加上可直接发送的 JSON 主体
// （成功负载或统一的 {"error":{...}} 结构）。
struct AuthResult {
    drogon::HttpStatusCode status = drogon::k500InternalServerError;
    nlohmann::json body;

    static AuthResult ok(nlohmann::json b) {
        AuthResult r;
        r.status = drogon::k200OK;
        r.body = std::move(b);
        return r;
    }

    static AuthResult error(drogon::HttpStatusCode s,
                            const std::string& msg) {
        AuthResult r;
        r.status = s;
        r.body = http_util::errorBodyForStatus(static_cast<int>(s), msg);
        return r;
    }
};

// 实现设计文档 六 中的小程序登录/绑定流程。
class AuthService {
public:
    using Callback = std::function<void(const AuthResult&)>;

    AuthService(std::shared_ptr<WechatClient> wechat,
                std::shared_ptr<GoBackendClient> goBackend,
                std::shared_ptr<WechatBindingRepository> bindings,
                long accessTtlSeconds,
                long openidTtlSeconds);

    // POST /api/miniapp/login {code}：jscode2session -> openid，然后要么签发
    // JWT（已绑定），要么签发短时效 openid_ticket（need_bind）。
    void login(const std::string& code, Callback cb);

    // POST /api/miniapp/bind：校验 openid_ticket -> openid，通过 go-backend
    // 校验凭证，持久化绑定，签发 JWT。
    void bind(const std::string& openidToken, const std::string& username,
              const std::string& password, Callback cb);

private:
    std::shared_ptr<WechatClient> wechat_;
    std::shared_ptr<GoBackendClient> goBackend_;
    std::shared_ptr<WechatBindingRepository> bindings_;
    long accessTtlSeconds_;
    long openidTtlSeconds_;
};
