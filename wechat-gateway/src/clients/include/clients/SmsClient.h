#pragma once

#include <chrono>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <unordered_map>

#include "clients/SmsProvider.h"

// 尝试发送验证码的结果。
struct SendCodeResult {
    bool ok = false;
    std::string code;    // 仅 mock 用：真实短信网关会投递它；real 模式恒为空。
                         // 生产环境中 API 响应绝不能返回它。
    std::string reason;  // 机器码（!ok 时非空）：
                         //   "rate_limited"            —— 60s 内重复发送
                         //   "provider_not_configured" —— real 模式但无 provider
                         //   "sms_send_failed"         —— provider 返回失败
    std::string errmsg;  // 人类可读详情（provider 的 errmsg / 本地描述），失败时非空
};

// 家长到校通知绑定流程的短信验证码客户端（设计文档 十一）。
//
// 为每个手机号存储一个 6 位验证码，带 TTL、每个验证码的尝试次数上限，以及
// 每个手机号的重发节流。验证码一次性使用：验证成功即
// 清除该条目；错误尝试会被计数，一旦达到上限验证码即失效。
// 不接受任何固定的后门验证码。
//
// 开发构建将验证码保存在进程内映射中；真实部署（realMode=true）则调用
// 阿里云/腾讯短信网关投递验证码（复用 notify.channels.sms 的 provider/签名/凭证）。
class SmsClient {
public:
    SmsClient();  // mock：无 provider，不真发（开发/测试）

    // real 模式：持有 provider，sendCode 时真发验证码短信；成功才存码供
    // verifyCode 校验，失败不存码（不占节流名额）。
    SmsClient(std::shared_ptr<SmsProvider> provider, bool realMode,
              std::string signName, std::string templateCode,
              std::string codeParam = "code");

    // 生成并向 `phone` “发送”一个 6 位验证码。强制执行重发
    // 节流（每个手机号每 kResendCooldownSeconds 一个验证码）；
    // 被节流时返回 ok=false 且 reason="rate_limited"。
    SendCodeResult sendCode(const std::string& phone);

    // 当 `code` 与发送给 `phone` 的验证码匹配（且在 TTL 内、
    // 未达尝试上限）时返回 true。成功时消耗该验证码。
    bool verifyCode(const std::string& phone, const std::string& code);

private:
    struct CodeEntry {
        std::string code;
        std::chrono::steady_clock::time_point expires_at;
        int attempts = 0;
        std::chrono::steady_clock::time_point last_sent_at;
    };

    std::string genCode();

    static constexpr int kCodeLength = 6;
    static constexpr int kCodeTtlSeconds = 5 * 60;       // 5 分钟
    static constexpr int kMaxAttempts = 5;
    static constexpr int kResendCooldownSeconds = 60;    // 每 60 秒 1 个验证码

    std::shared_ptr<SmsProvider> provider_;
    bool realMode_ = false;
    std::string signName_;
    std::string templateCode_;
    std::string codeParam_ = "code";

    std::mt19937 rng_;  // 由 std::random_device 播种（CSPRNG）
    std::mutex mutex_;
    std::unordered_map<std::string, CodeEntry> codes_;
};
