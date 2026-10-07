#include "clients/AliyunSmsProvider.h"

#include <drogon/HttpClient.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpTypes.h>
#include <nlohmann/json.hpp>
#include <trantor/utils/Logger.h>

#include <chrono>
#include <ctime>
#include <map>
#include <random>
#include <string>
#include <utility>

#include "utils/CryptoUtil.h"

namespace {

std::string iso8601Utc() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
}

std::string randomNonce() {
    static const char* hex = "0123456789abcdef";
    std::random_device rd;
    std::string s;
    s.reserve(32);
    for (int i = 0; i < 32; ++i) {
        s += hex[rd() % 16];
    }
    return s;
}

}  // namespace

AliyunSmsProvider::AliyunSmsProvider(std::string access_key_id,
                                     std::string access_key_secret,
                                     std::string region, std::string endpoint)
    : access_key_id_(std::move(access_key_id)),
      access_key_secret_(std::move(access_key_secret)),
      region_(std::move(region)),
      endpoint_(std::move(endpoint)) {}

SmsSendResult AliyunSmsProvider::send(const std::string& phone,
                                      const std::string& sign_name,
                                      const std::string& template_code,
                                      const nlohmann::json& params) {
    SmsSendResult out;

    // 公共参数 + 业务参数（std::map 保证按键排序）。
    std::map<std::string, std::string> q;
    q["AccessKeyId"] = access_key_id_;
    q["Action"] = "SendSms";
    q["Format"] = "JSON";
    q["PhoneNumbers"] = phone;
    q["RegionId"] = region_;
    q["SignName"] = sign_name;
    q["SignatureMethod"] = "HMAC-SHA1";
    q["SignatureNonce"] = randomNonce();
    q["SignatureVersion"] = "1.0";
    q["TemplateCode"] = template_code;
    q["TemplateParam"] = params.dump();
    q["Timestamp"] = iso8601Utc();
    q["Version"] = "2017-05-25";

    std::string canonical;
    for (const auto& [k, v] : q) {
        if (!canonical.empty()) {
            canonical += "&";
        }
        canonical += crypto::percentEncode(k) + "=" + crypto::percentEncode(v);
    }

    const std::string stringToSign =
        "GET&%2F&" + crypto::percentEncode(canonical);
    const std::string signature = crypto::base64Encode(
        crypto::hmacSha1(access_key_secret_ + "&", stringToSign));

    const std::string query =
        canonical + "&Signature=" + crypto::percentEncode(signature);

    auto client = drogon::HttpClient::newHttpClient(endpoint_);
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/?" + query);

    const auto [result, resp] = client->sendRequest(req);
    if (result != drogon::ReqResult::Ok || !resp) {
        out.errmsg = "aliyun sms request failed";
        return out;
    }

    const auto body = nlohmann::json::parse(resp->getBody(), nullptr, false);
    if (body.is_discarded() || !body.is_object()) {
        out.errmsg = "invalid aliyun sms response";
        return out;
    }

    const std::string code = body.value("Code", "");
    out.request_id = body.value("RequestId", "");
    if (code == "OK") {
        out.ok = true;
    } else {
        out.errmsg = body.value("Message", code);
        out.errcode = crypto::fnv1a32(code);
    }
    return out;
}
