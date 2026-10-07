#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// wechat_bindings 表的一行（见 sql/migration_v1.sql）。设计文档
// （六）只列出了 channel/openid/user_id，但这里也持久化了 role 和 name，
// 以便后续登录无需调用 go-backend 即可签发新的 JWT（否则会
// 破坏"本地验证，不回调 go-backend"原则）。
struct WechatBinding {
    std::string channel;  // "miniapp"
    std::string openid;
    std::string user_id;
    std::string role;  // 默认为 "user"
    std::string name;
};

class WechatBindingRepository {
public:
    virtual ~WechatBindingRepository() = default;

    virtual std::optional<WechatBinding> findByOpenid(
        const std::string& channel, const std::string& openid) const = 0;

    // 反查：user_id -> 绑定（用于推送时解析接收者的小程序 openid）。
    // wechat_bindings 有 (channel, user_id) 唯一约束，至多一行。
    virtual std::optional<WechatBinding> findByUser(
        const std::string& channel, const std::string& user_id) const = 0;

    // 幂等写入（upsert）(channel, openid) -> 绑定。
    virtual void save(const WechatBinding& binding) = 0;
};

// 内存 mock。
class InMemoryWechatBindingRepository : public WechatBindingRepository {
public:
    std::optional<WechatBinding> findByOpenid(
        const std::string& channel,
        const std::string& openid) const override {
        const auto it = byOpenid_.find(channel + ":" + openid);
        if (it == byOpenid_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    std::optional<WechatBinding> findByUser(
        const std::string& channel,
        const std::string& user_id) const override {
        for (const auto& [key, binding] : byOpenid_) {
            (void)key;
            if (binding.channel == channel && binding.user_id == user_id) {
                return binding;
            }
        }
        return std::nullopt;
    }

    void save(const WechatBinding& binding) override {
        byOpenid_[binding.channel + ":" + binding.openid] = binding;
    }

private:
    std::unordered_map<std::string, WechatBinding> byOpenid_;
};
