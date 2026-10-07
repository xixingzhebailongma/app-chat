#pragma once

#include <map>
#include <mutex>
#include <string>
#include <utility>

// 记录用户的订阅消息授权（设计文档 八 订阅授权）——模板级额度模型。
// quota = -1 长期订阅不限次；0 无授权；>0 一次性剩余次数；无行等价于 0。
class SubscriptionRepository {
public:
    virtual ~SubscriptionRepository() = default;

    // 授权授予：long_term 置 quota=-1；one_time 每次 accept +1。
    virtual void grant(const std::string& user_id,
                       const std::string& template_id, bool long_term) = 0;
    // 原子预扣（发送前）：有额度扣减并返回 true；-1(长期) 恒 true 不扣；
    // 0/无行返回 false。
    virtual bool reserve(const std::string& user_id,
                         const std::string& template_id) = 0;
    // 失败回补（仅 one_time 曾扣减的额度 +1；-1 不补）。
    virtual void refill(const std::string& user_id,
                        const std::string& template_id) = 0;
    // 拒收/退订/43101：该模板额度清零（只影响该模板）。
    virtual void revoke(const std::string& user_id,
                        const std::string& template_id) = 0;
    // 关闭订阅：清空该用户全部模板额度。
    virtual void revokeAll(const std::string& user_id) = 0;
    // 回显：返回该模板当前额度（-1/0/>0；无行 0）。
    virtual int quotaOf(const std::string& user_id,
                        const std::string& template_id) const = 0;
};

class InMemorySubscriptionRepository : public SubscriptionRepository {
public:
    void grant(const std::string& user_id, const std::string& template_id,
               bool long_term) override {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto k = key(user_id, template_id);
        if (long_term) {
            byUserTemplate_[k] = -1;
            return;
        }
        const auto it = byUserTemplate_.find(k);
        const int cur = (it == byUserTemplate_.end()) ? 0 : it->second;
        byUserTemplate_[k] = (cur < 0) ? cur : cur + 1;  // 长期(-1)不被覆盖
    }

    bool reserve(const std::string& user_id,
                 const std::string& template_id) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = byUserTemplate_.find(key(user_id, template_id));
        if (it == byUserTemplate_.end() || it->second == 0) {
            return false;
        }
        if (it->second > 0) {
            --it->second;  // one_time 扣减；-1 长期不扣
        }
        return true;
    }

    void refill(const std::string& user_id,
                const std::string& template_id) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = byUserTemplate_.find(key(user_id, template_id));
        if (it != byUserTemplate_.end() && it->second >= 0) {
            ++it->second;  // -1 长期不补
        }
    }

    void revoke(const std::string& user_id,
                const std::string& template_id) override {
        std::lock_guard<std::mutex> lock(mutex_);
        byUserTemplate_[key(user_id, template_id)] = 0;
    }

    void revokeAll(const std::string& user_id) override {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = byUserTemplate_.begin(); it != byUserTemplate_.end();) {
            if (it->first.first == user_id) {
                it = byUserTemplate_.erase(it);
            } else {
                ++it;
            }
        }
    }

    int quotaOf(const std::string& user_id,
                const std::string& template_id) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = byUserTemplate_.find(key(user_id, template_id));
        return (it == byUserTemplate_.end()) ? 0 : it->second;
    }

private:
    static std::pair<std::string, std::string> key(
        const std::string& user_id, const std::string& template_id) {
        return {user_id, template_id};
    }

    std::map<std::pair<std::string, std::string>, int> byUserTemplate_;
    mutable std::mutex mutex_;
};
