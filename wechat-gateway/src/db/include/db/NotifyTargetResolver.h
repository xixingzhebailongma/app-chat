#pragma once

#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// 通知方向的「user_id + channel -> 渠道外部地址」解析（设计文档 7.9 v2）。
// 与 db/NotifyTargetRepository.h（按角色/空间解析「收件人是谁」）是两个不同概念：
// 后者回答「发给谁」，本文件回答「这个人在该渠道的地址是什么」。

// 解析结果：有绑定 -> value 非空；无绑定 -> value=nullopt；查询/DB 异常 -> error 非空。
struct TargetResolution {
    std::optional<std::string> value;
    std::string error;
};

// 渠道地址解析（只读）。NotifyService 对 requiresTarget 的渠道统一调用，
// 把结果填进 ChannelPayload::target；渠道本身不感知数据来源。
class INotifyTargetResolver {
public:
    virtual ~INotifyTargetResolver() = default;
    virtual TargetResolution resolve(const std::string& channel,
                                     const std::string& user_id) = 0;
};

// user_notify_bindings 表的一行（供 admin 增删查）。
struct NotifyBinding {
    std::string channel;
    std::string user_id;
    std::string external_id;
    std::string external_extra;  // JSON 文本（渠道附加，可空 -> "{}"）
    std::string updated_at;      // 最近更新时间（PG 列表返回；内存实现为空串）
};

// 绑定表的管理写接口（admin 导入/维护）。与 INotifyTargetResolver 共用同一份
// user_notify_bindings 数据；实现上两者由同一个类承载（见 InMemory/Pg 实现），
// 避免 dev/PG 模式下读写状态分裂。
class NotifyBindingRepository {
public:
    virtual ~NotifyBindingRepository() = default;
    virtual bool save(const NotifyBinding& b) = 0;  // upsert
    virtual bool remove(const std::string& channel,
                        const std::string& user_id) = 0;
    virtual std::vector<NotifyBinding> list(const std::string& channel) = 0;
};

// 内存实现（开发/测试）。单实例既作 resolver 又作 binding 仓库。
class InMemoryNotifyTargetResolver : public INotifyTargetResolver,
                                     public NotifyBindingRepository {
public:
    TargetResolution resolve(const std::string& channel,
                             const std::string& user_id) override {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = store_.find({channel, user_id});
        if (it == store_.end()) {
            return TargetResolution{std::nullopt, ""};
        }
        return TargetResolution{it->second.external_id, ""};
    }

    bool save(const NotifyBinding& b) override {
        std::lock_guard<std::mutex> lock(mutex_);
        store_[{b.channel, b.user_id}] = b;
        return true;
    }

    bool remove(const std::string& channel,
                const std::string& user_id) override {
        std::lock_guard<std::mutex> lock(mutex_);
        return store_.erase({channel, user_id}) > 0;
    }

    std::vector<NotifyBinding> list(const std::string& channel) override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<NotifyBinding> out;
        for (const auto& [k, b] : store_) {
            if (k.first == channel) {
                out.push_back(b);
            }
        }
        return out;
    }

private:
    std::map<std::pair<std::string, std::string>, NotifyBinding> store_;
    std::mutex mutex_;
};
