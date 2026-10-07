#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

// 前置声明（hiredis）；真正的 include 保留在 .cpp 中，使此
// 头文件保持轻量。`struct redisReply` / `struct redisContext` 正是
// hiredis 所 typedef 的结构体标签。
struct redisContext;
struct redisReply;

// 面向微信 access_token 缓存（设计文档 十）和通知冷却
// （设计文档 九）的最小化 Redis 门面。
//
// 两种后端：
//   - 默认构造：进程本地 map，使开发构建和冒烟
//     测试无需 Redis 实例即可运行。
//   - RedisClient(host, port, password)：通过 hiredis（同步）访问真实 Redis，
//     使令牌缓存与刷新锁（`SET key ... NX EX`）在多个 pod 间共享。
//     公开接口保持精简——仅提供令牌管理器和 CooldownChecker 需要的命令。
class RedisClient {
public:
    RedisClient();   // 进程内回退
    RedisClient(std::string host, int port, std::string password = "");
    ~RedisClient();

    RedisClient(const RedisClient&) = delete;
    RedisClient& operator=(const RedisClient&) = delete;

    // `key` 对应的值；缺失 / 过期 / 出错时返回 ""。
    std::string get(const std::string& key);

    // 设置 `key` = `value`，并在 `ttlSeconds` 后过期。
    void set(const std::string& key, const std::string& value, long ttlSeconds);

    // `SET key value NX EX ttlSeconds`。仅当键原本不存在时返回 true——
    // 这就是刷新 / 冷却的分布式锁获取。
    bool setNx(const std::string& key, const std::string& value,
               long ttlSeconds);

    // 剩余 TTL（秒，>= 0）；缺失 / 过期 / 出错时返回 -1。
    long ttl(const std::string& key);

    void del(const std::string& key);

    // 探活：真实 Redis 发 PING，进程内回退恒 true。供健康检查端点使用。
    bool ping();

private:
    struct Entry {
        std::string value;
        std::chrono::steady_clock::time_point expiresAt;
    };

    // 真实 Redis 命令执行：首次使用时连接（并 AUTH），执行
    // `fmt`，并在重连后重试一次。失败时返回 nullptr。
    // 调用方必须持有 mutex_——hiredis 的 redisContext 非线程安全。
    struct redisReply* executeUnlocked(const char* fmt, ...);

    bool real_ = false;  // false => 进程内 map，true => hiredis
    std::string host_;
    int port_ = 6379;
    std::string password_;
    struct redisContext* ctx_ = nullptr;

    // 同时保护 hiredis 上下文与进程内 map。
    std::mutex mutex_;
    std::unordered_map<std::string, Entry> store_;
};
