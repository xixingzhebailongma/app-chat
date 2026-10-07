#include "clients/GoBackendClient.h"

#include <drogon/HttpClient.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpTypes.h>
#include <nlohmann/json.hpp>

#include <utility>

GoBackendClient::GoBackendClient(std::string baseUrl)
    : baseUrl_(std::move(baseUrl)) {}

void GoBackendClient::setInternalToken(std::string token) {
    internalToken_ = std::move(token);
}

void GoBackendClient::setApiPrefix(std::string prefix) {
    apiPrefix_ = std::move(prefix);
}

std::string GoBackendClient::getUserPhoneSync(const std::string& user_id) {
    const UpstreamConfig cfg = UpstreamConfig::instance();
    auto client = drogon::HttpClient::newHttpClient(baseUrl_);
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath(apiPrefix_ +
                 UpstreamConfig::fill(cfg.userPhonePath, {{"user_id", user_id}}));
    if (!internalToken_.empty()) {
        req->addHeader(cfg.userPhoneHeader, internalToken_);
    }

    const auto [result, resp] = client->sendRequest(req);
    if (result != drogon::ReqResult::Ok || !resp ||
        resp->getStatusCode() != drogon::k200OK) {
        return "";
    }

    const auto body = nlohmann::json::parse(resp->getBody(), nullptr, false);
    if (body.is_discarded() || !body.is_object() ||
        !body.contains(cfg.userPhoneKey) ||
        !body[cfg.userPhoneKey].is_string()) {
        return "";
    }
    return body[cfg.userPhoneKey].get<std::string>();
}

void GoBackendClient::login(const std::string& username,
                            const std::string& password, LoginCallback cb) {
    const UpstreamConfig cfg = UpstreamConfig::instance();
    auto client = drogon::HttpClient::newHttpClient(baseUrl_);
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath(apiPrefix_ + cfg.loginPath);
    req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    req->setBody(nlohmann::json{{cfg.loginReqUsername, username},
                                {cfg.loginReqPassword, password}}
                     .dump());

    client->sendRequest(
        req, [cb = std::move(cb), cfg](drogon::ReqResult result,
                                       const drogon::HttpResponsePtr& resp) {
            GoBackendLogin r;
            if (result != drogon::ReqResult::Ok || !resp) {
                r.ok = false;
                r.errmsg = "go-backend unreachable";
                cb(r);
                return;
            }

            auto body = nlohmann::json::parse(resp->getBody(), nullptr, false);
            if (resp->getStatusCode() != drogon::k200OK) {
                r.ok = false;
                // 尽力而为：从错误响应体中提取消息。
                if (!body.is_discarded() && body.is_object() &&
                    body.contains("message") && body["message"].is_string()) {
                    r.errmsg = body["message"].get<std::string>();
                } else {
                    r.errmsg = "invalid credentials";
                }
                cb(r);
                return;
            }

            if (body.is_discarded() || !body.is_object() ||
                !body.contains(cfg.loginUserKey) ||
                !body[cfg.loginUserKey].is_object()) {
                r.ok = false;
                r.errmsg = "go-backend response missing user object";
                cb(r);
                return;
            }

            const auto& user = body[cfg.loginUserKey];
            if (!user.contains(cfg.loginUserId)) {
                r.ok = false;
                r.errmsg = "go-backend response missing user.user_id";
                cb(r);
                return;
            }

            // user_id 在综合屏 API 契约中是 int，但可能以字符串形式返回；
            // 规范化为十进制字符串，以匹配网关的
            // VARCHAR(64) 约定（见 sql/migration_v1.sql）。
            const auto& uid = user[cfg.loginUserId];
            if (uid.is_number_integer()) {
                r.user_id = std::to_string(uid.get<long long>());
            } else if (uid.is_string()) {
                r.user_id = uid.get<std::string>();
            } else {
                r.ok = false;
                r.errmsg = "go-backend response user_id has invalid type";
                cb(r);
                return;
            }

            r.ok = true;
            r.username = user.value(cfg.loginUsername, "");
            r.name = user.value(cfg.loginName, "");
            r.role = user.value(cfg.loginRole, "teacher");
            if (r.role.empty()) {
                r.role = "teacher";
            }
            cb(r);
        });
}

void GoBackendClient::get(const std::string& path, const std::string& query,
                          const Headers& headers, RawCallback cb) {
    request(drogon::Get, path, query, "", headers, std::move(cb));
}

void GoBackendClient::post(const std::string& path, const std::string& body,
                           const Headers& headers, RawCallback cb) {
    request(drogon::Post, path, "", body, headers, std::move(cb));
}

void GoBackendClient::request(drogon::HttpMethod method,
                              const std::string& path,
                              const std::string& query,
                              const std::string& body,
                              const Headers& headers,
                              RawCallback cb) {
    auto client = drogon::HttpClient::newHttpClient(baseUrl_);
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(method);

    std::string fullPath = path;
    if (!query.empty()) {
        fullPath += "?" + query;
    }
    req->setPath(apiPrefix_ + fullPath);

    for (const auto& [key, value] : headers) {
        req->addHeader(key, value);
    }

    if (method == drogon::Post) {
        req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        req->setBody(body);
    }

    client->sendRequest(
        req, [cb = std::move(cb)](drogon::ReqResult result,
                                  const drogon::HttpResponsePtr& resp) {
            GoBackendResponse r;
            if (result != drogon::ReqResult::Ok || !resp) {
                r.ok = false;
                r.errmsg = "go-backend unreachable";
                cb(r);
                return;
            }

            r.ok = true;
            r.status = resp->getStatusCode();
            r.body = std::string(resp->getBody());
            r.contentType = resp->getContentType();
            cb(r);
        });
}
