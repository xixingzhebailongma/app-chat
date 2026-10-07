#include "clients/SmsClient.h"

#include <nlohmann/json.hpp>

#include <string>
#include <utility>

SmsClient::SmsClient() : rng_(std::random_device{}()) {}

SmsClient::SmsClient(std::shared_ptr<SmsProvider> provider, bool realMode,
                     std::string signName, std::string templateCode,
                     std::string codeParam)
    : provider_(std::move(provider)),
      realMode_(realMode),
      signName_(std::move(signName)),
      templateCode_(std::move(templateCode)),
      codeParam_(std::move(codeParam)),
      rng_(std::random_device{}()) {}

std::string SmsClient::genCode() {
    std::uniform_int_distribution<int> dist(0, 9);
    std::string code;
    code.reserve(kCodeLength);
    for (int i = 0; i < kCodeLength; ++i) {
        code.push_back(static_cast<char>('0' + dist(rng_)));
    }
    return code;
}

SendCodeResult SmsClient::sendCode(const std::string& phone) {
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(mutex_);

    SendCodeResult out;
    const auto it = codes_.find(phone);
    if (it != codes_.end() &&
        now - it->second.last_sent_at <
            std::chrono::seconds(kResendCooldownSeconds)) {
        out.reason = "rate_limited";
        return out;
    }

    const std::string code = genCode();

    // real 模式：真实短信网关投递，成功才存码供 verifyCode 校验；
    // 失败不存码（避免占用重发节流名额）。
    if (realMode_) {
        if (!provider_) {
            out.reason = "provider_not_configured";
            return out;
        }
        const SmsSendResult r = provider_->send(
            phone, signName_, templateCode_, nlohmann::json{{codeParam_, code}});
        if (!r.ok) {
            out.reason = "sms_send_failed";
            out.errmsg = r.errmsg;
            return out;
        }
    }

    CodeEntry entry;
    entry.code = code;
    entry.expires_at = now + std::chrono::seconds(kCodeTtlSeconds);
    entry.last_sent_at = now;
    codes_[phone] = std::move(entry);

    out.ok = true;
    if (!realMode_) {
        out.code = code;  // 仅开发用；real 模式由短信网关投递，不回显
    }
    return out;
}

bool SmsClient::verifyCode(const std::string& phone, const std::string& code) {
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = codes_.find(phone);
    if (it == codes_.end() || now >= it->second.expires_at) {
        if (it != codes_.end()) {
            codes_.erase(it);
        }
        return false;
    }

    if (it->second.attempts >= kMaxAttempts) {
        codes_.erase(it);
        return false;
    }

    ++it->second.attempts;
    const bool matched = (it->second.code == code);
    if (matched || it->second.attempts >= kMaxAttempts) {
        codes_.erase(it);
    }
    return matched;
}
