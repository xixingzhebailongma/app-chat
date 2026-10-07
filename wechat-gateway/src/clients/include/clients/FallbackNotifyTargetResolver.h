#pragma once

#include <memory>
#include <string>
#include <utility>

#include "clients/GoBackendClient.h"
#include "db/NotifyTargetResolver.h"

// sms 手机号的回落装饰器：底层查 user_notify_bindings；对 channel=="sms" 且
// 无绑定时回落到 go-backend 实时查询手机号。NotifyService 只拿解析结果，
// 不感知数据来源。resolve 里的 go-backend 查询为同步 HTTP，故只能在发送
// 工作线程上调用（与渠道 send 一致，绝不在 Drogon 事件循环线程）。
class FallbackNotifyTargetResolver : public INotifyTargetResolver {
public:
    FallbackNotifyTargetResolver(
        std::shared_ptr<INotifyTargetResolver> base,
        std::shared_ptr<GoBackendClient> goBackend)
        : base_(std::move(base)), goBackend_(std::move(goBackend)) {}

    TargetResolution resolve(const std::string& channel,
                             const std::string& user_id) override {
        TargetResolution r = base_->resolve(channel, user_id);
        if (r.value.has_value() || !r.error.empty()) {
            return r;  // 有绑定或查询出错 -> 原样返回
        }
        if (channel == "sms" && goBackend_) {
            const std::string phone = goBackend_->getUserPhoneSync(user_id);
            if (!phone.empty()) {
                return TargetResolution{phone, ""};
            }
        }
        return r;  // nullopt
    }

private:
    std::shared_ptr<INotifyTargetResolver> base_;
    std::shared_ptr<GoBackendClient> goBackend_;
};
