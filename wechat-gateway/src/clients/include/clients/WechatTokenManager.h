#pragma once

#include <memory>
#include <mutex>
#include <string>

#include "db/RedisClient.h"

// 微信 access_token 渠道（设计文档 十）。
enum class ChannelType {
    Miniapp,  // 小程序 access_token（订阅消息）
    Oa,       // 公众号 access_token（模板消息）
};

// 管理两个渠道的微信 access_token，缓存于 Redis
// （wechat:token:miniapp / wechat:token:oa），TTL 7200 秒，在过期前 300 秒
// 刷新，并使用分布式刷新锁（SET NX EX 10），确保同一时刻只有一个
// pod 请求微信（设计文档 十）。
//
// 40001 流程：收到 errcode 40001（令牌过期/被吊销）的渠道调用
// forceRefresh() 丢弃缓存并立即重新获取，随后再次 get()
// 拿到新令牌，并对该请求重试一次。invalidate() 保持为
// 仅丢弃缓存的变体，供调用方不能阻塞在刷新上时使用。
//
// NOTE：当缓存为空/即将过期时，get() 会同步刷新，因此
// 绝不能在 Drogon 的事件循环线程上运行（底层 HttpClient
// 对此有断言）。生产环境中刷新在工作线程上运行，或
// 令牌在启动时预热；当前的 mock 渠道不会调用它。
class WechatTokenManager {
public:
    struct Credentials {
        std::string appid;
        std::string secret;
    };

    WechatTokenManager(Credentials miniapp, Credentials oa,
                       std::shared_ptr<RedisClient> redis,
                       std::string apiBaseUrl = "https://api.weixin.qq.com");

    // `type` 的缓存 access_token，缺失或在过期前 kRefreshAheadSeconds 内时刷新。
    // 失败时返回 ""。
    std::string get(ChannelType type);

    // 丢弃缓存的令牌，以便下一次 get() 重新获取（40001 重试）。
    void invalidate(ChannelType type);

    // 在分布式锁下丢弃缓存并立即重新获取 ——
    // 即单次重试前的“40001 → 强制刷新”步骤。
    void forceRefresh(ChannelType type);

private:
    // 令牌获取的结果，携带足够信息以分类失败
    // （设计文档 13.8）：网络/瞬时错误 vs. 微信 errcode（40013/40164
    // 配置错误绝不能重试）。
    struct TokenFetch {
        std::string token;
        int errcode = 0;            // 微信返回错误码时为非零
        bool network_error = false; // 传输失败 / 响应格式错误
    };

    void refreshIfNeeded(ChannelType type);
    TokenFetch fetchFromWechat(ChannelType type);

    const Credentials& credentials(ChannelType type) const;
    static std::string cacheKey(ChannelType type);
    static std::string lockKey(ChannelType type);

    Credentials miniapp_;
    Credentials oa_;
    std::shared_ptr<RedisClient> redis_;
    std::string apiBaseUrl_;
    std::mutex mutex_;
};
