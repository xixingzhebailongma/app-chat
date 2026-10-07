#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#include "db/RedisClient.h"

inline constexpr long kAppTokenTtl = 7200;     // access_token 有效期
inline constexpr long kAppRefreshAhead = 300;  // 提前这么多秒刷新
inline constexpr long kAppLockTtl = 10;        // 分布式刷新锁 TTL
inline constexpr long kAppLockWaitMs = 200;    // 等待获胜 pod 写入

// 自管 access_token 的渠道（钉钉/企微）共用的 token 缓存 + 分布式刷新锁，
// 照 WechatTokenManager 的 Redis SET NX EX 写法。缓存/锁 key 由调用方指定，
// 带租户前缀（app_key / corp_id）避免多学校部署互踩。
class AppTokenCache {
public:
    // 返回 access_token；失败返回 ""。
    using Fetcher = std::function<std::string()>;

    AppTokenCache(std::shared_ptr<RedisClient> redis, std::string cacheKey,
                  std::string lockKey, Fetcher fetch)
        : redis_(redis ? std::move(redis) : std::make_shared<RedisClient>()),
          cacheKey_(std::move(cacheKey)),
          lockKey_(std::move(lockKey)),
          fetch_(std::move(fetch)) {}

    std::string get() {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::string cached = redis_->get(cacheKey_);
        if (!cached.empty() && redis_->ttl(cacheKey_) > kAppRefreshAhead) {
            return cached;
        }
        refreshLocked();
        return redis_->get(cacheKey_);
    }

    void forceRefresh() {
        std::lock_guard<std::mutex> lock(mutex_);
        redis_->del(cacheKey_);
        refreshLocked();
    }

private:
    void refreshLocked() {
        if (!redis_->setNx(lockKey_, "1", kAppLockTtl)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(kAppLockWaitMs));
            return;
        }
        const std::string cached = redis_->get(cacheKey_);
        if (!cached.empty() && redis_->ttl(cacheKey_) > kAppRefreshAhead) {
            redis_->del(lockKey_);
            return;
        }
        const std::string token = fetch_();
        if (!token.empty()) {
            redis_->set(cacheKey_, token, kAppTokenTtl);
        }
        redis_->del(lockKey_);
    }

    std::shared_ptr<RedisClient> redis_;
    std::string cacheKey_;
    std::string lockKey_;
    Fetcher fetch_;
    std::mutex mutex_;
};
