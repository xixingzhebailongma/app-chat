#pragma once

#include <openssl/hmac.h>
#include <openssl/sha.h>

#include <string>

// 短信服务商签名所需的底层原语（阿里云 HMAC-SHA1 + base64、
// 腾讯云 HMAC-SHA256 + hex、RFC3986 percent-encode）。
// 头文件内联实现，供 AliyunSmsProvider / TencentSmsProvider 复用，
// 避免与 JwtUtil.cpp 里匿名命名空间的 base64 重复。
namespace crypto {

inline std::string hmacSha256(const std::string& key, const std::string& data) {
    unsigned char out[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(), out,
         &len);
    return std::string(reinterpret_cast<char*>(out), len);
}

inline std::string hmacSha1(const std::string& key, const std::string& data) {
    unsigned char out[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    HMAC(EVP_sha1(), key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(), out,
         &len);
    return std::string(reinterpret_cast<char*>(out), len);
}

inline std::string sha256Hex(const std::string& data) {
    unsigned char out[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(data.data()), data.size(),
           out);
    static const char* hex = "0123456789abcdef";
    std::string s;
    s.reserve(SHA256_DIGEST_LENGTH * 2);
    for (unsigned char c : out) {
        s += hex[c >> 4];
        s += hex[c & 0xF];
    }
    return s;
}

// 标准 base64（带 padding）。
inline std::string base64Encode(const std::string& raw) {
    static const char* T =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const unsigned char* d =
        reinterpret_cast<const unsigned char*>(raw.data());
    const size_t len = raw.size();
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    size_t i = 0;
    for (; i + 2 < len; i += 3) {
        const unsigned n = (d[i] << 16) | (d[i + 1] << 8) | d[i + 2];
        out += T[(n >> 18) & 63];
        out += T[(n >> 12) & 63];
        out += T[(n >> 6) & 63];
        out += T[n & 63];
    }
    if (i + 1 == len) {
        const unsigned n = d[i] << 16;
        out += T[(n >> 18) & 63];
        out += T[(n >> 12) & 63];
        out += "==";
    } else if (i + 2 == len) {
        const unsigned n = (d[i] << 16) | (d[i + 1] << 8);
        out += T[(n >> 18) & 63];
        out += T[(n >> 12) & 63];
        out += T[(n >> 6) & 63];
        out += '=';
    }
    return out;
}

// FNV-1a 32 位哈希，把短信服务商的字符串错误码（阿里云 "isv.XXX" /
// 腾讯云 "FailedOperation.XXX"）映射为稳定的 int errcode 落库。
inline int fnv1a32(const std::string& s) {
    unsigned h = 2166136261u;
    for (unsigned char c : s) {
        h = (h ^ c) * 16777619u;
    }
    return static_cast<int>(h & 0x7fffffff);
}

// RFC3986 percent-encode：仅保留 A-Za-z0-9 - . _ ~，其余转 %XX（大写）。
inline std::string percentEncode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size() * 3);
    for (unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' ||
            c == '~') {
            out += static_cast<char>(c);
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0xF];
        }
    }
    return out;
}

}  // namespace crypto
