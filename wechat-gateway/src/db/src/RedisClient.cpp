#include "db/RedisClient.h"

#include <hiredis/hiredis.h>

#include <sys/time.h>

#include <cstdarg>
#include <utility>

namespace {
// 限制连接阶段耗时（不可达 / 无法路由的 Redis 不得挂起请求）。
const timeval kConnectTimeout = {1, 0};
}  // namespace

RedisClient::RedisClient() = default;

RedisClient::RedisClient(std::string host, int port, std::string password)
    : real_(true),
      host_(std::move(host)),
      port_(port),
      password_(std::move(password)) {}

RedisClient::~RedisClient() {
    if (ctx_) {
        redisFree(ctx_);
    }
}

redisReply* RedisClient::executeUnlocked(const char* fmt, ...) {
    // 在首次使用 / 上次失败后延迟建立连接（并 AUTH）。
    if (!ctx_) {
        ctx_ = redisConnectWithTimeout(host_.c_str(), port_, kConnectTimeout);
        if (!ctx_ || ctx_->err) {
            if (ctx_) {
                redisFree(ctx_);
                ctx_ = nullptr;
            }
            return nullptr;
        }
        if (!password_.empty()) {
            redisReply* auth =
                static_cast<redisReply*>(redisCommand(ctx_, "AUTH %s",
                                                      password_.c_str()));
            if (!auth || auth->type == REDIS_REPLY_ERROR) {
                if (auth) {
                    freeReplyObject(auth);
                }
                redisFree(ctx_);
                ctx_ = nullptr;
                return nullptr;
            }
            freeReplyObject(auth);
        }
    }

    va_list ap;
    va_start(ap, fmt);
    redisReply* reply = static_cast<redisReply*>(redisvCommand(ctx_, fmt, ap));
    va_end(ap);

    if (!reply && ctx_->err) {
        // 连接在传输中断开：重连（并重新 AUTH）后重放一次。
        if (redisReconnect(ctx_) == REDIS_OK) {
            if (!password_.empty()) {
                redisReply* auth =
                    static_cast<redisReply*>(redisCommand(ctx_, "AUTH %s",
                                                          password_.c_str()));
                if (!auth || auth->type == REDIS_REPLY_ERROR) {
                    if (auth) {
                        freeReplyObject(auth);
                    }
                    redisFree(ctx_);
                    ctx_ = nullptr;
                    return nullptr;
                }
                freeReplyObject(auth);
            }
            va_list ap2;
            va_start(ap2, fmt);
            reply = static_cast<redisReply*>(redisvCommand(ctx_, fmt, ap2));
            va_end(ap2);
        }
    }
    return reply;
}

std::string RedisClient::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!real_) {
        const auto it = store_.find(key);
        if (it == store_.end()) {
            return "";
        }
        if (std::chrono::steady_clock::now() >= it->second.expiresAt) {
            return "";
        }
        return it->second.value;
    }

    redisReply* reply = executeUnlocked("GET %s", key.c_str());
    if (!reply) {
        return "";
    }
    std::string out;
    if (reply->type == REDIS_REPLY_STRING) {
        out.assign(reply->str, reply->len);
    }
    freeReplyObject(reply);
    return out;
}

void RedisClient::set(const std::string& key, const std::string& value,
                      long ttlSeconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!real_) {
        Entry e;
        e.value = value;
        e.expiresAt = std::chrono::steady_clock::now() +
                      std::chrono::seconds(ttlSeconds);
        store_[key] = std::move(e);
        return;
    }

    redisReply* reply = executeUnlocked("SET %s %b EX %ld", key.c_str(),
                                        value.data(),
                                        static_cast<size_t>(value.size()),
                                        ttlSeconds);
    if (reply) {
        freeReplyObject(reply);
    }
}

bool RedisClient::setNx(const std::string& key, const std::string& value,
                        long ttlSeconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!real_) {
        const auto it = store_.find(key);
        if (it != store_.end() &&
            std::chrono::steady_clock::now() < it->second.expiresAt) {
            return false;  // 键存在且仍有效
        }
        Entry e;
        e.value = value;
        e.expiresAt = std::chrono::steady_clock::now() +
                      std::chrono::seconds(ttlSeconds);
        store_[key] = std::move(e);
        return true;
    }

    redisReply* reply = executeUnlocked("SET %s %b EX %ld NX", key.c_str(),
                                        value.data(),
                                        static_cast<size_t>(value.size()),
                                        ttlSeconds);
    // 失败放行：只有明确的 NIL（"键已存在"）才返回 false。
    // 连接错误（nullptr）或意外回复不得抑制
    // 通知 / 阻塞刷新——Redis 宕机时本来就没有共享
    // 状态可协调，因此继续执行是更小的恶。
    if (!reply || reply->type != REDIS_REPLY_NIL) {
        if (reply) {
            freeReplyObject(reply);
        }
        return true;
    }
    freeReplyObject(reply);
    return false;
}

long RedisClient::ttl(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!real_) {
        const auto it = store_.find(key);
        if (it == store_.end()) {
            return -1;
        }
        const long remaining =
            std::chrono::duration_cast<std::chrono::seconds>(
                it->second.expiresAt - std::chrono::steady_clock::now())
                .count();
        return remaining > 0 ? remaining : -1;
    }

    redisReply* reply = executeUnlocked("TTL %s", key.c_str());
    if (!reply) {
        return -1;
    }
    const long remaining = (reply->type == REDIS_REPLY_INTEGER)
                               ? static_cast<long>(reply->integer)
                               : -1;
    freeReplyObject(reply);
    return remaining;
}

void RedisClient::del(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!real_) {
        store_.erase(key);
        return;
    }

    redisReply* reply = executeUnlocked("DEL %s", key.c_str());
    if (reply) {
        freeReplyObject(reply);
    }
}

bool RedisClient::ping() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!real_) {
        return true;  // 进程内回退无需探活
    }
    redisReply* reply = executeUnlocked("PING");
    if (!reply) {
        return false;
    }
    const bool ok = (reply->type == REDIS_REPLY_STATUS &&
                     std::string(reply->str, reply->len) == "PONG");
    freeReplyObject(reply);
    return ok;
}
