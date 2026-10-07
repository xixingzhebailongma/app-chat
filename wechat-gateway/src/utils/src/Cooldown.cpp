#include "utils/Cooldown.h"

#include "db/RedisClient.h"

bool CooldownChecker::tryAcquire(const std::string& key, int ttl_seconds) {
    if (!redis_) {
        // 未接入存储（未接线的单元测试）-> 从不抑制。
        return true;
    }
    // Redis `SET key 1 EX ttl NX`（设计文档 9.3）：仅当键原本不存在时
    // setNx 才返回 true。
    return redis_->setNx(key, "1", ttl_seconds);
}
