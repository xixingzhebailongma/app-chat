#include "clients/TencentSmsProvider.h"

#include <drogon/HttpClient.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpTypes.h>
#include <nlohmann/json.hpp>
#include <trantor/utils/Logger.h>

#include <chrono>
#include <ctime>
#include <string>
#include <utility>

#include "utils/CryptoUtil.h"

namespace {

std::string dateUtc8() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
    return std::string(buf);
}

long unixSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

}  // namespace

TencentSmsProvider::TencentSmsProvider(std::string secret_id,
                                       std::string secret_key,
                                       std::string sdk_app_id,
                                       std::string region,
                                       std::string endpoint)
    : secret_id_(std::move(secret_id)),
      secret_key_(std::move(secret_key)),
      sdk_app_id_(std::move(sdk_app_id)),
      region_(std::move(region)),
      endpoint_(std::move(endpoint)) {
    const std::string::size_type scheme = endpoint_.find("://");
    host_ = scheme == std::string::npos
                ? endpoint_
                : endpoint_.substr(scheme + 3);
}

SmsSendResult TencentSmsProvider::send(const std::string& phone,
                                       const std::string& sign_name,
                                       const std::string& template_code,
                                       const nlohmann::json& params) {
    SmsSendResult out;

    nlohmann::json body;
    body["PhoneNumberSet"] = nlohmann::json::array({phone});
    body["SmsSdkAppId"] = sdk_app_id_;
    body["SignName"] = sign_name;
    body["TemplateId"] = template_code;
    nlohmann::json set = nlohmann::json::array();
    for (auto it = params.begin(); it != params.end(); ++it) {
        set.push_back(it.value());
    }
    body["TemplateParamSet"] = set;
    const std::string payload = body.dump();

    // TC3-HMAC-SHA256 签名。
    const std::string service = "sms";
    const std::string algorithm = "TC3-HMAC-SHA256";
    const long timestamp = unixSeconds();
    const std::string date = dateUtc8();

    const std::string canonicalHeaders =
        "content-type:application/json; charset=utf-8\nhost:" + host_ + "\n";
    const std::string signedHeaders = "content-type;host";
    const std::string canonicalRequest =
        "POST\n/\n\n" + canonicalHeaders + "\n" + signedHeaders + "\n" +
        crypto::sha256Hex(payload);

    const std::string credentialScope = date + "/" + service + "/tc3_request";
    const std::string stringToSign =
        algorithm + "\n" + std::to_string(timestamp) + "\n" + credentialScope +
        "\n" + crypto::sha256Hex(canonicalRequest);

    const std::string secretDate =
        crypto::hmacSha256("TC3" + secret_key_, date);
    const std::string secretService = crypto::hmacSha256(secretDate, service);
    const std::string secretSigning =
        crypto::hmacSha256(secretService, "tc3_request");
    const std::string signature = crypto::sha256Hex(
        crypto::hmacSha256(secretSigning, stringToSign));

    const std::string authorization =
        algorithm + " Credential=" + secret_id_ + "/" + credentialScope +
        ", SignedHeaders=" + signedHeaders + ", Signature=" + signature;

    auto client = drogon::HttpClient::newHttpClient(endpoint_);
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath("/");
    req->addHeader("Authorization", authorization);
    req->addHeader("Content-Type", "application/json; charset=utf-8");
    req->addHeader("Host", host_);
    req->addHeader("X-TC-Action", "SendSms");
    req->addHeader("X-TC-Timestamp", std::to_string(timestamp));
    req->addHeader("X-TC-Version", "2021-01-11");
    req->addHeader("X-TC-Region", region_);
    req->setBody(payload);

    const auto [result, resp] = client->sendRequest(req);
    if (result != drogon::ReqResult::Ok || !resp) {
        out.errmsg = "tencent sms request failed";
        return out;
    }

    const auto body2 = nlohmann::json::parse(resp->getBody(), nullptr, false);
    if (body2.is_discarded() || !body2.is_object() ||
        !body2.contains("Response")) {
        out.errmsg = "invalid tencent sms response";
        return out;
    }

    const auto& r = body2["Response"];
    out.request_id = r.value("RequestId", "");
    if (r.contains("Error")) {
        const auto& e = r["Error"];
        out.errmsg = e.value("Message", "");
        out.errcode = crypto::fnv1a32(e.value("Code", ""));
        return out;
    }

    const auto statuses = r.value("SendStatusSet", nlohmann::json::array());
    if (statuses.empty()) {
        out.errmsg = "empty SendStatusSet";
        return out;
    }
    bool allOk = true;
    for (const auto& s : statuses) {
        const std::string code = s.value("Code", "");
        if (code != "Ok") {
            allOk = false;
            if (out.errcode == 0) {
                out.errcode = crypto::fnv1a32(code);
                out.errmsg = s.value("Message", "");
            }
        }
    }
    out.ok = allOk;
    return out;
}
