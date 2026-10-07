#include "clients/WechatTokenManager.h"

#include <drogon/HttpClient.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpTypes.h>
#include <nlohmann/json.hpp>
#include <trantor/utils/Logger.h>

#include <chrono>
#include <thread>
#include <utility>

namespace {
constexpr long kTokenTtlSeconds = 7200;    // 微信 access_token 有效期
constexpr long kRefreshAheadSeconds = 300;  // 提前这么多秒在过期前刷新
constexpr long kLockTtlSeconds = 10;        // 分布式刷新锁的 TTL
constexpr long kLockWaitMs = 200;           // 等待获胜 pod 的写入
}  // namespace

WechatTokenManager::WechatTokenManager(Credentials miniapp, Credentials oa,
                                       std::shared_ptr<RedisClient> redis,
                                       std::string apiBaseUrl)
    : miniapp_(std::move(miniapp)),
      oa_(std::move(oa)),
      redis_(redis ? std::move(redis) : std::make_shared<RedisClient>()),
      apiBaseUrl_(std::move(apiBaseUrl)) {}

std::string WechatTokenManager::get(ChannelType type) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string key = cacheKey(type);

    const std::string cached = redis_->get(key);
    if (!cached.empty() && redis_->ttl(key) > kRefreshAheadSeconds) {
        return cached;
    }

    refreshIfNeeded(type);
    return redis_->get(key);
}

void WechatTokenManager::invalidate(ChannelType type) {
    std::lock_guard<std::mutex> lock(mutex_);
    redis_->del(cacheKey(type));
}

void WechatTokenManager::forceRefresh(ChannelType type) {
    std::lock_guard<std::mutex> lock(mutex_);
    redis_->del(cacheKey(type));
    refreshIfNeeded(type);
}

void WechatTokenManager::refreshIfNeeded(ChannelType type) {
    const std::string key = cacheKey(type);
    const std::string lockKeyStr = lockKey(type);

    // 分布式锁：只有一个 pod 刷新（设计文档 十 SET NX EX 10）。
    if (!redis_->setNx(lockKeyStr, "1", kLockTtlSeconds)) {
        // 另一个 pod 持有锁；给它一个先手，随后 get() 重新读取
        // 它写入的内容（如果仍不存在则回退到旧令牌）。
        std::this_thread::sleep_for(std::chrono::milliseconds(kLockWaitMs));
        return;
    }

    // 在锁内二次检查：并发的 get() 可能已经刷新了。
    const std::string cached = redis_->get(key);
    if (!cached.empty() && redis_->ttl(key) > kRefreshAheadSeconds) {
        redis_->del(lockKeyStr);
        return;
    }

    const TokenFetch fetched = fetchFromWechat(type);
    if (!fetched.token.empty()) {
        redis_->set(key, fetched.token, kTokenTtlSeconds);
    } else if (fetched.errcode == 40013 || fetched.errcode == 40164) {
        // 配置错误（appid 无效 / IP 未加入白名单）——不可重试；
        // 将其上报，以便运维修复凭据/IP（设计文档 13.8）。
        LOG_ERROR << "wechat token config error (errcode=" << fetched.errcode
                  << ") for channel " << static_cast<int>(type)
                  << "; check appid/secret/IP whitelist";
    }
    redis_->del(lockKeyStr);
}

WechatTokenManager::TokenFetch WechatTokenManager::fetchFromWechat(
    ChannelType type) {
    TokenFetch out;
    const Credentials& c = credentials(type);
    auto client = drogon::HttpClient::newHttpClient(apiBaseUrl_);
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/cgi-bin/token");
    req->setParameter("grant_type", "client_credential");
    req->setParameter("appid", c.appid);
    req->setParameter("secret", c.secret);

    // 同步 sendRequest（见头文件 NOTE：绝不要在事件循环线程上调用）。
    const auto [result, resp] = client->sendRequest(req);
    if (result != drogon::ReqResult::Ok || !resp) {
        out.network_error = true;  // 瞬时错误——调用方可使用旧令牌重试
        return out;
    }

    const auto body = nlohmann::json::parse(resp->getBody(), nullptr, false);
    if (body.is_discarded() || !body.is_object()) {
        out.network_error = true;
        return out;
    }
    if (body.contains("errcode")) {
        out.errcode = body["errcode"].get<int>();
        return out;  // 调用方进行分类（40013/40164 配置错误 vs. 瞬时错误）
    }
    if (!body.contains("access_token") || !body["access_token"].is_string()) {
        out.network_error = true;
        return out;
    }
    out.token = body["access_token"].get<std::string>();
    return out;
}

const WechatTokenManager::Credentials& WechatTokenManager::credentials(
    ChannelType type) const {
    switch (type) {
        case ChannelType::Miniapp:
            return miniapp_;
        case ChannelType::Oa:
            return oa_;
    }
    return oa_;  // 不可达
}

std::string WechatTokenManager::cacheKey(ChannelType type) {
    switch (type) {
        case ChannelType::Miniapp:
            return "wechat:token:miniapp";
        case ChannelType::Oa:
            return "wechat:token:oa";
    }
    return "";
}

std::string WechatTokenManager::lockKey(ChannelType type) {
    return cacheKey(type) + ":lock";
}
