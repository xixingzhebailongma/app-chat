#pragma once

#include <memory>
#include <string>

#include "clients/SmsProvider.h"

namespace sms {

// 按服务商名 + 凭证构造短信 provider（阿里云/腾讯云）。access_key/secret 遵循
// 「Secret 只放 K8s Secret」约定：SMS_ACCESS_KEY / SMS_SECRET 环境变量覆盖传入
// 值（含配置里的 ${...} 占位符）。未知名回退 aliyun（与 SmsChannel 现状一致）。
// 供 notify.sms 渠道工厂与 OA 家长绑定验证码短信共用，避免复制粘贴。
std::shared_ptr<SmsProvider> makeProvider(const std::string& provider,
                                          std::string accessKey,
                                          std::string secret,
                                          const std::string& region,
                                          const std::string& sdkAppId);

}  // namespace sms
