#include "filters/InternalTokenFilter.h"

#include <openssl/crypto.h>

#include <drogon/HttpRequest.h>

#include "utils/HttpResponseUtil.h"

std::string InternalTokenFilter::token_;

void InternalTokenFilter::doFilter(const drogon::HttpRequestPtr& req,
                                   drogon::FilterCallback&& fcb,
                                   drogon::FilterChainCallback&& fccb) {
    const auto header = req->getHeader("X-Internal-Token");
    // 恒定时间比较（CRYPTO_memcmp），使共享内部令牌无法
    // 通过时间侧信道被恢复。
    const bool valid =
        !token_.empty() && header.size() == token_.size() &&
        CRYPTO_memcmp(header.data(), token_.data(), token_.size()) == 0;
    if (!valid) {
        fcb(http_util::error(drogon::k401Unauthorized,
                             "invalid or missing X-Internal-Token"));
        return;
    }
    fccb();
}
