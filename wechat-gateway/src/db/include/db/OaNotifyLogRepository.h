#pragma once

#include <string>
#include <vector>

// oa_notify_logs 表的一行（sql/migration_v10.sql，设计文档 7.8 §7）。
// 本阶段只写事件级日志（parent_openid 用占位 '__event_level__'）；事件级
// event_id 技术幂等已落地（existsByEventId），逐家长 pending→success/failed
// 更新等真实发送接入后再启用。
struct OaNotifyLogEntry {
    std::string event_id;
    std::string parent_openid = "__event_level__";
    std::string student_no;
    std::string space_id;      // 请求暂缺 space_id，先留空
    std::string event_type = "arrival";
    std::string status;        // skipped / no_parent_binding / success / failed / ...
    std::string reason;        // channel_disabled / mock / no_parent_binding / ""
    int errcode = 0;           // 渠道返回错误码（结构化落库，不再折叠进 reason）
};

// event_id 技术幂等查询的三态结果：Absent=无记录，Present=已有记录（重复），
// Error=查询失败（PG 抖动等），视为可重试错误、不能当「不存在」继续发送。
enum class EventIdCheck { Absent, Present, Error };

// 当日业务去重三态：Absent=当日无成功记录，Present=当日已成功推过（跳过），
// Error=查询失败（可重试，fail-closed 保「至多推一次」）。
enum class DedupCheck { Absent, Present, Error };

// 到校通知日志持久化。由 OaNotifyService::notify 在入口写一条事件级日志。
class OaNotifyLogRepository {
public:
    virtual ~OaNotifyLogRepository() = default;

    virtual void save(const OaNotifyLogEntry& entry) = 0;

    // event_id 技术幂等：是否已存在任一记录（事件级，不按 status 区分）。
    // 依赖「failed 视为终态、不重发、不入重试队列」的边侧契约；契约变更须改按 status 过滤。
    virtual EventIdCheck existsByEventId(const std::string& event_id) const = 0;

    // 当日业务去重：当天 (student_no, space_id) 是否已有 status='success' 记录。
    virtual DedupCheck hasSuccessToday(const std::string& student_no,
                                       const std::string& space_id) const = 0;
};

// 内存 mock；仅纯内存演示/测试用，真实落库见 PgOaNotifyLogRepository。
class InMemoryOaNotifyLogRepository : public OaNotifyLogRepository {
public:
    void save(const OaNotifyLogEntry& entry) override {
        entries_.push_back(entry);
    }

    EventIdCheck existsByEventId(const std::string& event_id) const override {
        if (force_error_) {
            return EventIdCheck::Error;  // 测试钩子：模拟查询失败
        }
        for (const auto& e : entries_) {
            if (e.event_id == event_id) {
                return EventIdCheck::Present;
            }
        }
        return EventIdCheck::Absent;
    }

    // 测试钩子：置位后 existsByEventId 直接返回 Error（生产内存模式不触发）。
    DedupCheck hasSuccessToday(const std::string& student_no,
                               const std::string& space_id) const override {
        if (force_dedup_error_) {
            return DedupCheck::Error;  // 测试钩子：模拟去重查询失败
        }
        // 内存实现忽略「当日」时间维度（无真实时钟），只匹配 (student_no, space_id)
        // + status=success；跨日场景由 Pg 实现的时间过滤覆盖。
        for (const auto& e : entries_) {
            if (e.student_no == student_no && e.space_id == space_id &&
                e.status == "success") {
                return DedupCheck::Present;
            }
        }
        return DedupCheck::Absent;
    }

    void setForceError(bool b) { force_error_ = b; }

    // 测试钩子：置位后 hasSuccessToday 直接返回 Error（生产内存模式不触发）。
    void setForceDedupError(bool b) { force_dedup_error_ = b; }

    const std::vector<OaNotifyLogEntry>& entries() const { return entries_; }

private:
    std::vector<OaNotifyLogEntry> entries_;
    bool force_error_ = false;
    bool force_dedup_error_ = false;
};
