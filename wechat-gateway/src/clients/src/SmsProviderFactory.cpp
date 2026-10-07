#include "clients/SmsProviderFactory.h"

#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

#include "clients/AliyunSmsProvider.h"
#include "clients/TencentSmsProvider.h"

namespace sms {

std::shared_ptr<SmsProvider> makeProvider(const std::string& provider,
                                          std::string accessKey,
                                          std::string secret,
                                          const std::string& region,
                                          const std::string& sdkAppId) {
    // 短信密钥遵循「Secret 只放 K8s Secret」约定：环境变量覆盖配置占位符。
    if (const char* e = std::getenv("SMS_ACCESS_KEY"); e && *e) {
        accessKey = e;
    }
    if (const char* e = std::getenv("SMS_SECRET"); e && *e) {
        secret = e;
    }

    if (provider == "tencent") {
        return std::make_shared<TencentSmsProvider>(accessKey, secret, sdkAppId,
                                                    region);
    }
    return std::make_shared<AliyunSmsProvider>(accessKey, secret, region);
}

}  // namespace sms
