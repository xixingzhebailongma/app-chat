#pragma once

#include <nlohmann/json.hpp>

#include <string>

// 一次短信发送的结果（服务商返回的 Code / Message 映射）。
struct SmsSendResult {
    bool ok = false;
    int errcode = 0;            // 服务商错误码（非 0 为失败）
    std::string errmsg;         // 服务商 message 或本地错误描述
    std::string request_id;     // 服务商回执 id（可选，用于排查）
};

// 短信服务商抽象（设计文档 9.5）。SmsChannel 通过该接口发送，
// 具体服务商由 config: notify.channels.sms.provider 选择。
class SmsProvider {
public:
    virtual ~SmsProvider() = default;

    // "aliyun" / "tencent"
    virtual std::string name() const = 0;

    // 发送单条短信。`params` 为 JSON 对象（模板变量名 -> 值）。
    // 阿里云直接作为 TemplateParam 序列化；腾讯云按值转为 TemplateParamSet。
    virtual SmsSendResult send(const std::string& phone,
                               const std::string& sign_name,
                               const std::string& template_code,
                               const nlohmann::json& params) = 0;
};
