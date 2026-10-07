#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include "channels/INotifyChannel.h"

class WechatTokenManager;
class GoBackendClient;
class SubscriptionRepository;
class RedisClient;

// 单个渠道的配置（7.9 插件化）。`raw` 是 notify.channels.<name> 的原始 JSON
// 对象，由各渠道 builder 自行解析；nlohmann 默认构造是 null 而非 object，
// 因此 builder 读 raw 一律要容错（见 channelCfgGet）。
struct ChannelConfig {
    bool enabled = true;
    nlohmann::json raw;
};

// 各渠道共享的构造依赖（只放跨渠道共享设施；各渠道凭证经 builder 的 c.raw
// 读取，不进 deps）。builder 只「捕获」这些指针、不在构造期解引用或调用，
// 从而工厂测试可传空 deps 只验类型；生产路径在发送时才真正用到它们。
struct ChannelDeps {
    std::shared_ptr<WechatTokenManager> tokens;
    std::shared_ptr<GoBackendClient> goBackend;
    std::string wechatApiBaseUrl;
    std::shared_ptr<SubscriptionRepository> subscriptionRepo;  // wechat_miniapp 额度用
    std::shared_ptr<RedisClient> redis;                        // dingtalk/wecom 自管 token 缓存用
};

// 渠道工厂：名字 -> builder 的注册表。新增渠道 = 实现 INotifyChannel +
// 在渠道 .cpp 里 registerBuilder 一行 + config 加条目（不再改 main.cpp /
// NotifyService 核心逻辑）。builder 由各渠道 .cpp 的静态初始化注册，main 之前
// 即就绪；Meyers 单例规避静态初始化顺序问题。
class ChannelFactory {
public:
    using Builder = std::function<std::shared_ptr<INotifyChannel>(
        const ChannelConfig&, const ChannelDeps&)>;

    static ChannelFactory& instance();

    void registerBuilder(std::string name, Builder builder);

    // 未注册该名字时返回 nullptr（调用方据此 LOG_WARN 跳过）。
    std::shared_ptr<INotifyChannel> build(const std::string& name,
                                          const ChannelConfig&,
                                          const ChannelDeps&) const;

private:
    std::map<std::string, Builder> builders_;
};

// 容错读 raw 的字符串字段：raw 可能为 null（默认构造）或缺失该键，统一
// 用 is_object 守卫，绝不抛 type_error。
inline std::string channelCfgGet(const nlohmann::json& raw,
                                 const std::string& key,
                                 const std::string& def = "") {
    if (!raw.is_object()) {
        return def;
    }
    return raw.value(key, def);
}
