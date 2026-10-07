#pragma once

#include <algorithm>
#include <ctime>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// alert_handles 表的一行（见 sql/migration_v1.sql）。
struct Alert {
    std::string alert_id;
    std::string space_id;
    std::string event_type;
    std::string status;  // 状态值：unhandled / handling / resolved
    std::string operator_id;
    std::string operator_name;  // 由 LEFT JOIN wechat_bindings 解析（仅 admin 返回非空）
    std::string remark;
    std::string device_id;     // 关联设备（空串 = 无关联设备，如纯传感器告警）
    std::string device_label;  // 设备展示名（告警卡片展示用）
    std::string created_at;  // ISO8601（TIMESTAMPTZ，默认 now()）
    std::string updated_at;
};

// 告警历史查询的非空间筛选条件（7.5④ 按时间/类型筛选）。空字符串 = 该维度
// 不过滤。空间范围由 query/count 的 space_ids 参数单独表达（见下）。
struct AlertFilter {
    std::string event_type;
    std::string status;
    std::string from;  // created_at >= from（ISO8601）
    std::string to;    // created_at <= to（ISO8601）
};

// 告警统计（7.5④）：按状态/类型/教室聚合计数。全量统计，不带时间/类型/状态筛选。
struct AlertStats {
    std::map<std::string, int> by_status;      // status -> 计数
    std::map<std::string, int> by_event_type;  // event_type -> 计数
    std::map<std::string, int> by_space;       // space_id -> 计数（仅 admin 用）
    std::map<std::string, int> by_space_unhandled;  // 未处理告警按教室分布（7.5① 高亮用）
};

class AlertRepository {
public:
    virtual ~AlertRepository() = default;

    // 分页查询：space_ids 为空 = 不过滤（仅 admin 场景）；否则 space_id IN (...)。
    // 按 created_at DESC, alert_id DESC 稳定排序，截取 [offset, offset+limit)。
    // limit < 0 表示不限制。
    virtual std::vector<Alert> query(const std::vector<std::string>& space_ids,
                                     const AlertFilter& f, int limit,
                                     int offset) const = 0;

    // 与 query 相同筛选条件下的总数。
    virtual int count(const std::vector<std::string>& space_ids,
                      const AlertFilter& f) const = 0;

    // 告警入库（producer 侧）：已存在同 alert_id 则跳过（DO NOTHING，不覆盖
    // 管理员已做的处理状态/操作人/备注）。返回本次 DB 操作是否成功（重复
    // event_id 跳过不算失败）。
    virtual bool upsert(const Alert& alert) = 0;

    // 设置告警状态（handling / resolved），并记录处理人。
    // 当 alert_id 未知时返回 false。
    virtual bool handle(const std::string& alert_id,
                        const std::string& operator_id,
                        const std::string& remark,
                        const std::string& status) = 0;

    // 按主键取一行；不存在返回 nullopt。handle() 写运维日志前取
    // space_id / event_type 用。
    virtual std::optional<Alert> findById(const std::string& alert_id) const = 0;

    // 告警统计（7.5④）：按状态/类型/教室聚合计数。space_ids 为空 = 不过滤
    // （仅 admin 场景）；否则 space_id IN (...)。
    virtual AlertStats stats(const std::vector<std::string>& space_ids) const = 0;
};

// 内存 mock。在开发装配 / 测试中通过 add() 播种数据。
class InMemoryAlertRepository : public AlertRepository {
public:
    void add(const Alert& alert) { byId_[alert.alert_id] = alert; }

    // 操作人姓名种子（仅内存模式）：handle 时按 operator_id 解析 operator_name，
    // 对齐 PG 模式「LEFT JOIN wechat_bindings 解析 operator_name」的行为。
    void setName(const std::string& user_id, const std::string& name) {
        names_[user_id] = name;
    }

    std::vector<Alert> query(const std::vector<std::string>& space_ids,
                             const AlertFilter& f, int limit,
                             int offset) const override {
        std::vector<Alert> filtered;
        for (const auto& [_, alert] : byId_) {
            if (match(alert, space_ids, f)) {
                filtered.push_back(alert);
            }
        }
        std::sort(filtered.begin(), filtered.end(),
                  [](const Alert& a, const Alert& b) {
                      if (a.created_at != b.created_at) {
                          return a.created_at > b.created_at;
                      }
                      return a.alert_id > b.alert_id;
                  });
        std::vector<Alert> out;
        for (size_t i = 0; i < filtered.size(); ++i) {
            if (static_cast<int>(i) < offset) {
                continue;
            }
            if (limit >= 0 && static_cast<int>(out.size()) >= limit) {
                break;
            }
            out.push_back(std::move(filtered[i]));
        }
        return out;
    }

    int count(const std::vector<std::string>& space_ids,
              const AlertFilter& f) const override {
        int n = 0;
        for (const auto& [_, alert] : byId_) {
            if (match(alert, space_ids, f)) {
                ++n;
            }
        }
        return n;
    }

    bool upsert(const Alert& alert) override {
        if (byId_.count(alert.alert_id) == 0) {
            Alert a = alert;
            if (a.created_at.empty()) {
                a.created_at = nowIso();
            }
            if (a.updated_at.empty()) {
                a.updated_at = a.created_at;
            }
            byId_[a.alert_id] = a;  // 首次写入
        }
        // 已存在则跳过（不覆盖处理状态）；内存无 I/O 错误，恒返回成功。
        return true;
    }

    bool handle(const std::string& alert_id,
                const std::string& operator_id,
                const std::string& remark,
                const std::string& status) override {
        const auto it = byId_.find(alert_id);
        if (it == byId_.end()) {
            return false;
        }
        it->second.status = status;
        it->second.operator_id = operator_id;
        it->second.operator_name =
            names_.count(operator_id) ? names_.at(operator_id) : "";
        it->second.remark = remark;
        it->second.updated_at = nowIso();
        return true;
    }

    std::optional<Alert> findById(const std::string& alert_id) const override {
        const auto it = byId_.find(alert_id);
        if (it == byId_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    AlertStats stats(const std::vector<std::string>& space_ids) const override {
        AlertStats s;
        for (const auto& [_, alert] : byId_) {
            if (!space_ids.empty() &&
                std::find(space_ids.begin(), space_ids.end(), alert.space_id) ==
                    space_ids.end()) {
                continue;
            }
            s.by_status[alert.status]++;
            s.by_event_type[alert.event_type]++;
            s.by_space[alert.space_id]++;
            if (alert.status == "unhandled") {
                s.by_space_unhandled[alert.space_id]++;
            }
        }
        return s;
    }

private:
    // 内存模式的 ISO-8601 UTC 时间戳（对齐 PG 的 now() 默认值）。
    static std::string nowIso() {
        std::time_t now = std::time(nullptr);
        std::tm t{};
#if defined(_WIN32)
        gmtime_s(&t, &now);
#else
        gmtime_r(&now, &t);
#endif
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &t);
        return std::string(buf);
    }

    static bool match(const Alert& a, const std::vector<std::string>& space_ids,
                      const AlertFilter& f) {
        if (!space_ids.empty() &&
            std::find(space_ids.begin(), space_ids.end(), a.space_id) ==
                space_ids.end()) {
            return false;
        }
        if (!f.event_type.empty() && a.event_type != f.event_type) {
            return false;
        }
        if (!f.status.empty() && a.status != f.status) {
            return false;
        }
        if (!f.from.empty() && a.created_at < f.from) {
            return false;
        }
        if (!f.to.empty() && a.created_at > f.to) {
            return false;
        }
        return true;
    }

    std::unordered_map<std::string, Alert> byId_;
    std::map<std::string, std::string> names_;  // user_id -> 姓名（内存模式解析 operator_name）
};
