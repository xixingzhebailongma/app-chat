#pragma once

#include <memory>
#include <string>

class RedisClient;

// 在冷却窗口内抑制同一 device+event_type 的重复通知
// （设计文档 9.2/9.3）。
//
// tryAcquire(key, ttl) 是"检查并设置"：当键尚未处于
// 冷却中（且现在开始冷却）时返回 true，否则返回 false。它委托给
// RedisClient::setNx，使开发内存 mock 与未来的 hiredis 后端
// 客户端共享同一代码路径——`SET key 1 EX ttl NX`。
class CooldownChecker {
public:
    explicit CooldownChecker(std::shared_ptr<RedisClient> redis = nullptr)
        : redis_(std::move(redis)) {}

    // 为 `key` 获取 `ttl_seconds` 时长的冷却槽位。获取成功
    // （未在冷却中）时返回 true，键仍在冷却中时返回 false。
    bool tryAcquire(const std::string& key, int ttl_seconds);

private:
    std::shared_ptr<RedisClient> redis_;
};

// 冷却键的单一事实来源（设计文档 7.6 / 9.2）。按
// `event_type` + `device_id` + `user` + `channel` 去重——
// 每个订阅者、每个渠道各收到一次（防单人重复轰炸，非防全局重复）。
// 单独成函数便于单测锁死格式，防止被改回「全局只推一次」的旧语义。
namespace cooldown {

inline std::string key(const std::string& event_type,
                       const std::string& device_id,
                       const std::string& user,
                       const std::string& channel) {
    return "cooldown:" + event_type + ":" + device_id + ":" + user + ":" +
           channel;
}

}  // namespace cooldown
