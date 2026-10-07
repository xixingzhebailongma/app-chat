#include "utils/JwtUtil.h"

#include <openssl/crypto.h>
#include <openssl/hmac.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdint>
#include <string>

namespace {

using nlohmann::json;

// ---- base64url (RFC 4648 §5) ----
constexpr char kB64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64Encode(const std::string& raw) {
    const unsigned char* data =
        reinterpret_cast<const unsigned char*>(raw.data());
    const size_t len = raw.size();
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    size_t i = 0;
    for (; i + 2 < len; i += 3) {
        const unsigned n = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
        out += kB64[(n >> 18) & 63];
        out += kB64[(n >> 12) & 63];
        out += kB64[(n >> 6) & 63];
        out += kB64[n & 63];
    }
    if (i + 1 == len) {
        const unsigned n = data[i] << 16;
        out += kB64[(n >> 18) & 63];
        out += kB64[(n >> 12) & 63];
        out += "==";
    } else if (i + 2 == len) {
        const unsigned n = (data[i] << 16) | (data[i + 1] << 8);
        out += kB64[(n >> 18) & 63];
        out += kB64[(n >> 12) & 63];
        out += kB64[(n >> 6) & 63];
        out += '=';
    }
    return out;
}

// 解码到 out。每次输出字节后，`buf` 都会被掩码为剩余未消费的位，
// 因此无论输入多长都不会溢出。
bool base64Decode(const std::string& in, std::string& out) {
    int table[256];
    for (int i = 0; i < 256; ++i) {
        table[i] = -1;
    }
    for (int i = 0; i < 64; ++i) {
        table[static_cast<unsigned char>(kB64[i])] = i;
    }

    out.clear();
    out.reserve((in.size() / 4) * 3 + 3);
    std::uint32_t buf = 0;
    int nbits = 0;
    for (unsigned char c : in) {
        if (c == '=') {
            break;
        }
        const int v = table[c];
        if (v < 0) {
            return false;  // 无效的 base64 字符
        }
        buf = (buf << 6) | static_cast<std::uint32_t>(v);
        nbits += 6;
        if (nbits >= 8) {
            nbits -= 8;
            out.push_back(static_cast<char>((buf >> nbits) & 0xFF));
            buf &= (1u << nbits) - 1u;  // 此处 nbits 位于 [0, 6]
        }
    }
    return true;
}

std::string b64urlEncode(const std::string& raw) {
    std::string s = base64Encode(raw);
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '=') {
            break;
        }
        if (c == '+') {
            out += '-';
        } else if (c == '/') {
            out += '_';
        } else {
            out += c;
        }
    }
    return out;
}

bool b64urlDecode(const std::string& in, std::string& out) {
    std::string s = in;
    for (char& c : s) {
        if (c == '-') {
            c = '+';
        } else if (c == '_') {
            c = '/';
        }
    }
    while (s.size() % 4 != 0) {
        s += '=';
    }
    return base64Decode(s, out);
}

std::string hmacSha256(const std::string& data, const std::string& key) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(),
         digest, &len);
    return std::string(reinterpret_cast<char*>(digest), len);
}

bool constTimeEqual(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) {
        return false;
    }
    return CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
}

long nowEpochSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string sign(const json& header, const json& payload,
                 const std::string& secret) {
    const std::string h = b64urlEncode(header.dump());
    const std::string p = b64urlEncode(payload.dump());
    const std::string input = h + "." + p;
    const std::string sig = b64urlEncode(hmacSha256(input, secret));
    return input + "." + sig;
}

// 校验签名 + exp，成功时返回解析后的负载。
std::optional<json> verify(const std::string& token,
                           const std::string& secret) {
    const size_t dot1 = token.find('.');
    if (dot1 == std::string::npos) {
        return std::nullopt;
    }
    const size_t dot2 = token.find('.', dot1 + 1);
    if (dot2 == std::string::npos) {
        return std::nullopt;
    }
    const std::string h = token.substr(0, dot1);
    const std::string p = token.substr(dot1 + 1, dot2 - dot1 - 1);
    const std::string sig = token.substr(dot2 + 1);
    if (h.empty() || p.empty() || sig.empty()) {
        return std::nullopt;
    }

    const std::string input = h + "." + p;
    const std::string expected = b64urlEncode(hmacSha256(input, secret));
    if (!constTimeEqual(expected, sig)) {
        return std::nullopt;
    }

    std::string payloadJson;
    if (!b64urlDecode(p, payloadJson)) {
        return std::nullopt;
    }
    auto payload = json::parse(payloadJson, nullptr, false);
    if (payload.is_discarded() || !payload.is_object()) {
        return std::nullopt;
    }
    if (!payload.contains("exp") || !payload["exp"].is_number_integer()) {
        return std::nullopt;
    }
    if (payload["exp"].get<long>() <= nowEpochSeconds()) {
        return std::nullopt;
    }
    return payload;
}

}  // namespace

std::string JwtUtil::secret_;

void JwtUtil::setSecret(const std::string& secret) { secret_ = secret; }

std::string JwtUtil::signAccessToken(const std::string& user_id,
                                     const std::string& role,
                                     long ttlSeconds) {
    const json header = {{"alg", "HS256"}, {"typ", "JWT"}};
    const json payload = {{"user_id", user_id},
                          {"role", role},
                          {"exp", nowEpochSeconds() + ttlSeconds}};
    return sign(header, payload, secret_);
}

std::optional<AccessClaims> JwtUtil::verifyAccessToken(
    const std::string& token) {
    const auto payload = verify(token, secret_);
    if (!payload) {
        return std::nullopt;
    }
    if (!payload->contains("user_id") || !(*payload)["user_id"].is_string() ||
        !payload->contains("role") || !(*payload)["role"].is_string()) {
        return std::nullopt;
    }
    AccessClaims claims;
    claims.user_id = (*payload)["user_id"].get<std::string>();
    claims.role = (*payload)["role"].get<std::string>();
    claims.exp = (*payload)["exp"].get<long>();
    return claims;
}

std::string JwtUtil::signOpenidTicket(const std::string& openid,
                                      long ttlSeconds) {
    const json header = {{"alg", "HS256"}, {"typ", "JWT"}};
    const json payload = {{"openid", openid},
                          {"token_type", "openid_ticket"},
                          {"exp", nowEpochSeconds() + ttlSeconds}};
    return sign(header, payload, secret_);
}

std::optional<std::string> JwtUtil::verifyOpenidTicket(
    const std::string& token) {
    const auto payload = verify(token, secret_);
    if (!payload) {
        return std::nullopt;
    }
    if (!payload->contains("openid") || !(*payload)["openid"].is_string()) {
        return std::nullopt;
    }
    if ((*payload).value("token_type", "") != "openid_ticket") {
        return std::nullopt;
    }
    return (*payload)["openid"].get<std::string>();
}
