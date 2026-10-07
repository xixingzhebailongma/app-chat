#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

// access_records 表的一行（见 sql/migration_v6.sql）：一条人脸/刷卡进出记录。
// 存储层保留 user_id（供将来按人统计），但 API 响应不返回。
struct AccessRecord {
    std::int64_t id;          // 稳定二级排序键（Pg BIGSERIAL / InMemory 自增）
    std::string event_id;     // 边侧事件唯一 ID（幂等键）
    std::string space_id;
    std::string user_id;      // 存储不返回
    std::string name;
    std::string auth_type;    // face / card
    std::string result;       // login / denied
    std::string device_id;
    std::string occurred_at;  // ISO8601
};

class AccessRecordRepository {
public:
    virtual ~AccessRecordRepository() = default;

    // event_id 幂等写入：已存在（重推）则跳过。
    virtual void insert(const AccessRecord& r) = 0;

    // space_ids 为空 = 不过滤（仅 admin 场景）；否则 space_id IN (...)。
    // authType 为空 = 不过滤；否则 auth_type = 指定值（face/card）。
    // 按 occurred_at DESC, id DESC 稳定排序，截取 [offset, offset+limit)。
    virtual std::vector<AccessRecord> query(
        const std::vector<std::string>& space_ids,
        const std::optional<std::string>& from,
        const std::optional<std::string>& to, int limit, int offset,
        const std::optional<std::string>& authType = std::nullopt) const = 0;

    // 与 query 相同过滤条件下的总数。
    virtual int count(const std::vector<std::string>& space_ids,
                      const std::optional<std::string>& from,
                      const std::optional<std::string>& to,
                      const std::optional<std::string>& authType = std::nullopt) const = 0;
};

// 内存 mock（开发/测试）。
class InMemoryAccessRecordRepository : public AccessRecordRepository {
public:
    void insert(const AccessRecord& r) override {
        if (seen_.count(r.event_id) > 0) {
            return;  // 幂等：重推跳过
        }
        seen_.insert(r.event_id);
        AccessRecord copy = r;
        copy.id = next_id_++;
        records_.push_back(std::move(copy));
    }

    std::vector<AccessRecord> query(
        const std::vector<std::string>& space_ids,
        const std::optional<std::string>& from,
        const std::optional<std::string>& to, int limit, int offset,
        const std::optional<std::string>& authType = std::nullopt) const override {
        std::vector<AccessRecord> filtered;
        for (const auto& r : records_) {
            if (!match(r, space_ids, from, to, authType)) {
                continue;
            }
            filtered.push_back(r);
        }
        std::sort(filtered.begin(), filtered.end(),
                  [](const AccessRecord& a, const AccessRecord& b) {
                      if (a.occurred_at != b.occurred_at) {
                          return a.occurred_at > b.occurred_at;
                      }
                      return a.id > b.id;
                  });
        std::vector<AccessRecord> out;
        for (int i = 0; i < static_cast<int>(filtered.size()); ++i) {
            if (i < offset) {
                continue;
            }
            if (limit >= 0 &&
                static_cast<int>(out.size()) >= limit) {
                break;
            }
            out.push_back(std::move(filtered[static_cast<size_t>(i)]));
        }
        return out;
    }

    int count(const std::vector<std::string>& space_ids,
              const std::optional<std::string>& from,
              const std::optional<std::string>& to,
              const std::optional<std::string>& authType = std::nullopt) const override {
        int n = 0;
        for (const auto& r : records_) {
            if (match(r, space_ids, from, to, authType)) {
                ++n;
            }
        }
        return n;
    }

private:
    static bool match(const AccessRecord& r,
                      const std::vector<std::string>& space_ids,
                      const std::optional<std::string>& from,
                      const std::optional<std::string>& to,
                      const std::optional<std::string>& authType) {
        if (!space_ids.empty()) {
            if (std::find(space_ids.begin(), space_ids.end(), r.space_id) ==
                space_ids.end()) {
                return false;
            }
        }
        if (from && r.occurred_at < *from) {
            return false;
        }
        if (to && r.occurred_at > *to) {
            return false;
        }
        if (authType && r.auth_type != *authType) {
            return false;
        }
        return true;
    }

    std::unordered_set<std::string> seen_;
    std::vector<AccessRecord> records_;
    std::int64_t next_id_ = 1;
};
