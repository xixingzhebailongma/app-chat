#pragma once

#include <string>

#include "clients/SmsProvider.h"

// 阿里云短信（Dysmsapi SendSms，RPC 签名 V1.0：HMAC-SHA1 + base64）。
// 同步调用（drogon HttpClient 阻塞 sendRequest）——绝不能在 Drogon
// 事件循环线程上调用（同 WechatTokenManager 的约定）。
class AliyunSmsProvider : public SmsProvider {
public:
    AliyunSmsProvider(std::string access_key_id, std::string access_key_secret,
                      std::string region = "cn-hangzhou",
                      std::string endpoint = "https://dysmsapi.aliyuncs.com");

    std::string name() const override { return "aliyun"; }
    SmsSendResult send(const std::string& phone, const std::string& sign_name,
                       const std::string& template_code,
                       const nlohmann::json& params) override;

private:
    std::string access_key_id_;
    std::string access_key_secret_;
    std::string region_;
    std::string endpoint_;
};
