#pragma once

#include <cstdint>
#include <string>
#include <vector>

// operation_logs 表的一行（见 sql/migration_v8.sql）：一条运维操作日志，
// 与 7.5 运维日志统一。op_type 通用化：scene_execute（场景控制）、
// alert_handle（告警处置）共用同一张表。
struct OperationLogEntry {
    std::string op_type;  // "scene_execute" / "alert_handle"
    std::string user_id;
    std::string space_id;
    std::string scene_id;
    int success_count = 0;
    int failed_count = 0;
    std::string detail;  // JSON 字符串（场景：每台设备的结果；告警：处置摘要）
};

// operation_logs 查询结果的一行（读接口 GET /api/miniapp/operation-logs）。
struct OperationLogRow {
    std::int64_t id = 0;
    std::string op_type;
    std::string user_id;
    std::string operator_name;  // 由 LEFT JOIN wechat_bindings 解析
    std::string space_id;
    std::string scene_id;
    int success_count = 0;
    int failed_count = 0;
    std::string detail;     // JSONB 原文（服务层负责解析）
    std::string created_at; // ISO8601
};

// 读接口的非空间筛选条件；空字符串 = 该维度不过滤。
struct OperationLogFilter {
    std::string op_type;
    std::string space_id;
    std::string user_id;
    std::string alert_id;  // detail->>'alert_id' 过滤（告警详情时间线用）
    std::string from;  // created_at >= from
    std::string to;    // created_at <= to
};

// 运维操作日志仓库。仅保留 Pg 实现（无 InMemory）：
// InMemory 的 insert 恒返回 true，会吞掉「PG 完全宕机」这类失败、
// 绕过 SceneService 的文件兜底，与「记录不丢」的承诺冲突。
// insert 返回是否成功，由调用方决定是否走文件兜底。
class OperationLogRepository {
public:
    virtual ~OperationLogRepository() = default;
    virtual bool insert(const OperationLogEntry& e) = 0;

    // 分页查询（按 id DESC 稳定排序）；offset 从 0 起。
    virtual std::vector<OperationLogRow> query(const OperationLogFilter& f,
                                               int limit,
                                               int offset) const = 0;
    // 与 query 相同筛选条件下的总数。
    virtual int count(const OperationLogFilter& f) const = 0;
};
