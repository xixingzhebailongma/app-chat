#pragma once

#include <drogon/HttpFilter.h>
#include <string>

// 通过 X-Internal-Token 请求头鉴权 /internal/*。
class InternalTokenFilter : public drogon::HttpFilter<InternalTokenFilter> {
public:
    void doFilter(const drogon::HttpRequestPtr& req,
                  drogon::FilterCallback&& fcb,
                  drogon::FilterChainCallback&& fccb) override;

    static void setToken(const std::string& token) { token_ = token; }

private:
    static std::string token_;
};
