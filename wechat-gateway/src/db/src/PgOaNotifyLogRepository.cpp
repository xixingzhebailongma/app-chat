#include "db/PgOaNotifyLogRepository.h"

#include <libpq-fe.h>

#include <utility>

#include "db/PgPool.h"

PgOaNotifyLogRepository::PgOaNotifyLogRepository(std::shared_ptr<PgPool> pool)
    : pool_(std::move(pool)) {}

void PgOaNotifyLogRepository::save(const OaNotifyLogEntry& entry) {
    // 事件级日志：parent_openid 用占位 '__event_level__'；唯一键
    // (event_id, parent_openid) 保证同一事件至多一条事件级日志。
    // space_id 暂缺写 NULL。ON CONFLICT DO NOTHING 兜底：事件级日志至多一条，
    // 重复 event_id 不会因唯一键冲突而报错（前置 existsByEventId 已拦截重复发送）。
    const std::string sql =
        "INSERT INTO oa_notify_logs (event_id, parent_openid, student_no,"
        " space_id, event_type, status, reason, errcode)"
        " VALUES ($1, $2, $3, NULLIF($4, ''), $5, $6, NULLIF($7, ''), $8)"
        " ON CONFLICT (event_id, parent_openid) DO NOTHING";
    struct pg_result* res = pool_->execParams(
        sql, {entry.event_id, entry.parent_openid, entry.student_no,
              entry.space_id, entry.event_type, entry.status, entry.reason,
              std::to_string(entry.errcode)});
    pool_->clear(res);
}

EventIdCheck PgOaNotifyLogRepository::existsByEventId(
    const std::string& event_id) const {
    // 事件级幂等：任一记录即「已处理」（不按 status 区分）。依赖「failed 视为
    // 终态、不重发、不入重试队列」的边侧契约；若契约改为 failed 可重发，须改
    // 为按 status 过滤（如 WHERE ... AND status <> 'failed'）。
    const std::string sql =
        "SELECT 1 FROM oa_notify_logs WHERE event_id = $1 LIMIT 1";
    struct pg_result* res = pool_->execParams(sql, {event_id});
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
        pool_->clear(res);
        return EventIdCheck::Error;  // 查询失败视为可重试错误，不当作「不存在」
    }
    const bool present = PQntuples(res) > 0;
    pool_->clear(res);
    return present ? EventIdCheck::Present : EventIdCheck::Absent;
}

DedupCheck PgOaNotifyLogRepository::hasSuccessToday(
    const std::string& student_no, const std::string& space_id) const {
    // 当日业务去重：当天（Asia/Shanghai 自然日）该 (student_no, space_id) 已有
    // status='success' 记录则拦截。只拦成功，skipped/failed/no_parent_binding 不拦。
    // 时区硬编码 Asia/Shanghai；多时区部署需提为配置。
    const std::string sql =
        "SELECT 1 FROM oa_notify_logs"
        " WHERE student_no = $1 AND space_id = $2 AND status = 'success'"
        "   AND (created_at AT TIME ZONE 'Asia/Shanghai')::date"
        "     = (CURRENT_TIMESTAMP AT TIME ZONE 'Asia/Shanghai')::date"
        " LIMIT 1";
    struct pg_result* res = pool_->execParams(sql, {student_no, space_id});
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
        pool_->clear(res);
        return DedupCheck::Error;  // 查询失败 fail-closed，保「至多推一次」
    }
    const bool present = PQntuples(res) > 0;
    pool_->clear(res);
    return present ? DedupCheck::Present : DedupCheck::Absent;
}
