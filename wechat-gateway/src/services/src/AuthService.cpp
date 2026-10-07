#include "services/AuthService.h"

#include <drogon/drogon.h>

#include <string>

#include "utils/JwtUtil.h"

AuthService::AuthService(std::shared_ptr<WechatClient> wechat,
                         std::shared_ptr<GoBackendClient> goBackend,
                         std::shared_ptr<WechatBindingRepository> bindings,
                         long accessTtlSeconds,
                         long openidTtlSeconds)
    : wechat_(std::move(wechat)),
      goBackend_(std::move(goBackend)),
      bindings_(std::move(bindings)),
      accessTtlSeconds_(accessTtlSeconds),
      openidTtlSeconds_(openidTtlSeconds) {}

void AuthService::login(const std::string& code, Callback cb) {
    wechat_->jscode2session(code, [this, cb](const WechatSession& s) {
        if (!s.ok) {
            cb(AuthResult::error(drogon::k401Unauthorized,
                                 s.errmsg.empty() ? "invalid code" : s.errmsg));
            return;
        }

        const auto binding = bindings_->findByOpenid("miniapp", s.openid);
        if (binding) {
            const std::string token = JwtUtil::signAccessToken(
                binding->user_id, binding->role, accessTtlSeconds_);
            cb(AuthResult::ok(nlohmann::json{{"token", token},
                                             {"user_id", binding->user_id},
                                             {"role", binding->role},
                                             {"name", binding->name}}));
            return;
        }

        const std::string ticket =
            JwtUtil::signOpenidTicket(s.openid, openidTtlSeconds_);
        cb(AuthResult::ok(
            nlohmann::json{{"need_bind", true}, {"openid_token", ticket}}));
    });
}

void AuthService::bind(const std::string& openidToken,
                       const std::string& username,
                       const std::string& password, Callback cb) {
    const auto openid = JwtUtil::verifyOpenidTicket(openidToken);
    if (!openid) {
        cb(AuthResult::error(drogon::k401Unauthorized,
                             "invalid or expired openid_token"));
        return;
    }

    goBackend_->login(
        username, password, [this, cb, openid = *openid](const GoBackendLogin& r) {
            if (!r.ok) {
                cb(AuthResult::error(drogon::k401Unauthorized,
                                     r.errmsg.empty() ? "invalid credentials"
                                                      : r.errmsg));
                return;
            }

            WechatBinding b;
            b.channel = "miniapp";
            b.openid = openid;
            b.user_id = r.user_id;
            b.role = r.role.empty() ? "teacher" : r.role;
            b.name = r.name;
            if (b.role != "teacher" && b.role != "admin") {
                LOG_WARN << "bind: unexpected role '" << b.role << "' for user "
                         << r.user_id << "; falling back to teacher";
                b.role = "teacher";
            }
            bindings_->save(b);

            const std::string token = JwtUtil::signAccessToken(
                b.user_id, b.role, accessTtlSeconds_);
            cb(AuthResult::ok(nlohmann::json{{"token", token},
                                             {"user_id", b.user_id},
                                             {"role", b.role},
                                             {"name", b.name}}));
        });
}
