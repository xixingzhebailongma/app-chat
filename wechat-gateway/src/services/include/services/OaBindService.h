#pragma once

#include <drogon/HttpTypes.h>
#include <nlohmann/json.hpp>

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "clients/SmsClient.h"
#include "clients/WechatClient.h"
#include "db/ParentBindingRepository.h"
#include "db/StudentParentRepository.h"
#include "utils/ErrorBody.h"

// 返回给 OaController 绑定处理器的结果（与 AuthResult 结构相同：
// 一个 HTTP 状态码加上可直接发送的 JSON 主体）。
struct OaBindResult {
    drogon::HttpStatusCode status = drogon::k500InternalServerError;
    nlohmann::json body;

    static OaBindResult ok(nlohmann::json b) {
        OaBindResult r;
        r.status = drogon::k200OK;
        r.body = std::move(b);
        return r;
    }

    static OaBindResult error(drogon::HttpStatusCode s,
                              const std::string& msg) {
        OaBindResult r;
        r.status = s;
        r.body = http_util::errorBodyForStatus(static_cast<int>(s), msg);
        return r;
    }
};

// 家长到校通知 — 绑定流程（设计文档 十一）。
//
// 家长关注公众号 → H5 网页授权拿 code → /authorize 换 openid 并下发短时效
// openid_ticket → 家长填学号/手机号/验证码 → /confirm 校验后写入
// parent_student_bindings.
class OaBindService {
public:
    using Callback = std::function<void(const OaBindResult&)>;

    OaBindService(std::shared_ptr<WechatClient> wechat,
                  std::shared_ptr<SmsClient> sms,
                  std::shared_ptr<ParentBindingRepository> bindings,
                  std::shared_ptr<StudentParentRepository> studentParents);

    // GET /api/oa/bind/authorize?code=xxx：网页授权 code -> OA openid，然后下发
    // 短时效 openid_ticket，由 H5 携带进入 /confirm。
    void authorize(const std::string& code, Callback cb);

    // POST /api/oa/bind/send-code：生成验证码并"发送"到手机。
    // 对重发进行限流（被限流时返回 429）。
    void sendCode(const std::string& phone, Callback cb);

    // POST /api/oa/bind/confirm：校验 openid_ticket + 短信验证码，持久化
    // parent_student_bindings 行。
    void confirm(const std::string& openidToken, const std::string& studentNo,
                 const std::string& phone, const std::string& smsCode,
                 Callback cb);

    // GET /api/oa/bind/me?openid_token=：校验票据，返回当前 openid 已绑定的
    // 孩子列表（多孩子模型，未绑定返回 bound:false）。
    void me(const std::string& openidToken, Callback cb);

    // POST /api/oa/bind/unbind：校验票据，删除指定学号的绑定。
    void unbind(const std::string& openidToken, const std::string& studentNo,
                Callback cb);

private:
    std::shared_ptr<WechatClient> wechat_;
    std::shared_ptr<SmsClient> sms_;
    std::shared_ptr<ParentBindingRepository> bindings_;
    std::shared_ptr<StudentParentRepository> studentParents_;
};
