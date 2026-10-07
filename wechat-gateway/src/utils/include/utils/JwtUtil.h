#pragma once

#include <optional>
#include <string>

// go-backend 访问令牌解码后的声明。
struct AccessClaims {
    std::string user_id;
    std::string role;
    long exp = 0;
};

// 使用共享的 go-backend 密钥进行 HS256 JWT 签名/校验（设计文档 六）。
//
// - 访问令牌携带 {user_id, role, exp}——与 go-backend 的
//   负载一致——默认 72 小时有效期。
// - openid_ticket 是短期（5 分钟）的网关内部票据，仅
//   携带 {openid, token_type, exp}。它使用同一密钥签名，但
//   绝不会被误认为访问令牌（verifyAccessToken 要求
//   user_id/role；verifyOpenidTicket 要求 openid + token_type）。
class JwtUtil {
public:
    static constexpr long DEFAULT_ACCESS_TTL = 72 * 3600;  // 72 小时
    static constexpr long DEFAULT_OPENID_TTL = 5 * 60;     // 5 分钟

    // 必须在启动时以共享密钥调用一次。
    static void setSecret(const std::string& secret);

    static std::string signAccessToken(const std::string& user_id,
                                       const std::string& role,
                                       long ttlSeconds = DEFAULT_ACCESS_TTL);

    // 签名无效或令牌已过期时返回 nullopt。
    static std::optional<AccessClaims> verifyAccessToken(
        const std::string& token);

    static std::string signOpenidTicket(const std::string& openid,
                                        long ttlSeconds = DEFAULT_OPENID_TTL);

    // 成功时返回 openid，无效/过期/类型错误时返回 nullopt。
    static std::optional<std::string> verifyOpenidTicket(
        const std::string& token);

private:
    static std::string secret_;
};
