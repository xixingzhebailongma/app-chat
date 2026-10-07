#include "services/OaBindService.h"

#include <trantor/utils/Logger.h>

#include <utility>

#include "utils/JwtUtil.h"

OaBindService::OaBindService(
    std::shared_ptr<WechatClient> wechat, std::shared_ptr<SmsClient> sms,
    std::shared_ptr<ParentBindingRepository> bindings,
    std::shared_ptr<StudentParentRepository> studentParents)
    : wechat_(std::move(wechat)),
      sms_(std::move(sms)),
      bindings_(std::move(bindings)),
      studentParents_(std::move(studentParents)) {}

void OaBindService::sendCode(const std::string& phone, Callback cb) {
    if (!sms_) {
        cb(OaBindResult::error(drogon::k500InternalServerError,
                               "sms client unavailable"));
        return;
    }

    const SendCodeResult r = sms_->sendCode(phone);
    if (!r.ok) {
        if (r.reason == "rate_limited") {
            cb(OaBindResult::error(drogon::k429TooManyRequests,
                                   "please wait before requesting another code"));
        } else if (r.reason == "provider_not_configured") {
            cb(OaBindResult::error(drogon::k500InternalServerError,
                                   "sms provider not configured"));
        } else {  // sms_send_failed 或未知失败：给 H5 明确错误提示
            LOG_ERROR << "oa bind sms send failed phone=" << phone
                      << " err=" << r.errmsg;
            cb(OaBindResult::error(drogon::k502BadGateway,
                                   "短信发送失败，请稍后重试"));
        }
        return;
    }

    // 生产环境中绝不可将验证码返回给客户端 —— 由真实短信网关负责下发。
    // 此处仅为开发日志而回显。
    cb(OaBindResult::ok(nlohmann::json{{"sent", true}}));
}

void OaBindService::authorize(const std::string& code, Callback cb) {
    if (!wechat_) {
        cb(OaBindResult::error(drogon::k500InternalServerError,
                               "wechat client unavailable"));
        return;
    }

    wechat_->oauth2AccessToken(code, [this, cb](const WechatOauthResult& r) {
        if (!r.ok) {
            cb(OaBindResult::error(
                drogon::k401Unauthorized,
                r.errmsg.empty() ? "invalid code" : r.errmsg));
            return;
        }

        const std::string ticket = JwtUtil::signOpenidTicket(r.openid);
        cb(OaBindResult::ok(
            nlohmann::json{{"openid", r.openid}, {"openid_token", ticket}}));
    });
}

void OaBindService::confirm(const std::string& openidToken,
                            const std::string& studentNo,
                            const std::string& phone,
                            const std::string& smsCode, Callback cb) {
    const auto openid = JwtUtil::verifyOpenidTicket(openidToken);
    if (!openid) {
        cb(OaBindResult::error(drogon::k401Unauthorized,
                               "invalid or expired openid_token"));
        return;
    }

    // 花名册校验（设计文档 7.8 §6.2）：学号存在 + 手机号匹配，均为 status=active。
    if (studentParents_ && !studentParents_->hasActiveStudent(studentNo)) {
        cb(OaBindResult::error(drogon::k400BadRequest, "student_no not found"));
        return;
    }
    if (studentParents_ &&
        !studentParents_->matchesActiveParent(studentNo, phone)) {
        cb(OaBindResult::error(drogon::k400BadRequest, "phone not matched"));
        return;
    }

    if (sms_ && !sms_->verifyCode(phone, smsCode)) {
        cb(OaBindResult::error(drogon::k400BadRequest, "invalid sms_code"));
        return;
    }

    ParentBinding b;
    b.student_no = studentNo;
    b.parent_openid_oa = *openid;
    b.phone = phone;
    if (bindings_) {
        bindings_->save(b);
    }

    cb(OaBindResult::ok(nlohmann::json{{"bound", true},
                                       {"student_no", studentNo},
                                       {"phone", phone}}));
}

void OaBindService::me(const std::string& openidToken, Callback cb) {
    const auto openid = JwtUtil::verifyOpenidTicket(openidToken);
    if (!openid) {
        cb(OaBindResult::error(drogon::k401Unauthorized,
                               "invalid or expired openid_token"));
        return;
    }

    nlohmann::json bindings = nlohmann::json::array();
    if (bindings_) {
        for (const auto& b : bindings_->findBindingsByOpenid(*openid)) {
            nlohmann::json item{
                {"student_no", b.student_no},
                {"phone", b.phone},
            };
            // bound_at：有 verified_at 才输出（内存仓库可能为空，前端回退）。
            if (!b.verified_at.empty()) {
                item["bound_at"] = b.verified_at;
            }
            // student_name：join 花名册，缺失省略（前端回退只显示学号）。
            if (studentParents_) {
                const std::string name =
                    studentParents_->findStudentName(b.student_no);
                if (!name.empty()) {
                    item["student_name"] = name;
                }
            }
            bindings.push_back(std::move(item));
        }
    }

    cb(OaBindResult::ok(nlohmann::json{{"bound", !bindings.empty()},
                                       {"bindings", std::move(bindings)}}));
}

void OaBindService::unbind(const std::string& openidToken,
                           const std::string& studentNo, Callback cb) {
    const auto openid = JwtUtil::verifyOpenidTicket(openidToken);
    if (!openid) {
        cb(OaBindResult::error(drogon::k401Unauthorized,
                               "invalid or expired openid_token"));
        return;
    }

    const bool removed = bindings_ && bindings_->remove(*openid, studentNo);
    if (!removed) {
        cb(OaBindResult::error(drogon::k404NotFound, "binding not found"));
        return;
    }

    cb(OaBindResult::ok(nlohmann::json{{"unbound", true},
                                       {"student_no", studentNo}}));
}
