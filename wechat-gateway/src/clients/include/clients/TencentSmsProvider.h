#pragma once

#include <string>

#include "clients/SmsProvider.h"

// 腾讯云短信（sms.tencentcloudapi.com SendSms，TC3-HMAC-SHA256 签名）。
// 同步调用（drogon HttpClient 阻塞 sendRequest）——绝不能在 Drogon
// 事件循环线程上调用。
class TencentSmsProvider : public SmsProvider {
public:
    TencentSmsProvider(std::string secret_id, std::string secret_key,
                       std::string sdk_app_id,
                       std::string region = "ap-guangzhou",
                       std::string endpoint = "https://sms.tencentcloudapi.com");

    std::string name() const override { return "tencent"; }
    SmsSendResult send(const std::string& phone, const std::string& sign_name,
                       const std::string& template_code,
                       const nlohmann::json& params) override;

private:
    std::string secret_id_;
    std::string secret_key_;
    std::string sdk_app_id_;
    std::string region_;
    std::string endpoint_;
    std::string host_;  // 从 endpoint 派生，用于签名与 Host 头
};
