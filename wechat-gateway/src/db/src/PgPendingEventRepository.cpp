#include "db/PgPendingEventRepository.h"

#include <libpq-fe.h>

#include <string>
#include <utility>

#include "db/PgPool.h"

namespace {

// 当语句返回元组（SELECT ... RETURNING）或命令标签
// （INSERT/UPDATE/DELETE）时，视为"成功"。
bool okStatus(PGresult* res) {
    if (!res) {
        return false;
    }
    const ExecStatusType s = PQresultStatus(res);
    return s == PGRES_TUPLES_OK || s == PGRES_COMMAND_OK;
}

}  // namespace

PgPendingEventRepository::PgPendingEventRepository(std::shared_ptr<PgPool> pool,
                                                   std::string claimed_by)
    : pool_(std::move(pool)), claimed_by_(std::move(claimed_by)) {}

void PgPendingEventRepository::enqueue(const nlohmann::json& payload) {
    const std::string sql =
        "INSERT INTO pending_events (payload, status, next_retry_at)"
        " VALUES ($1::jsonb, 'pending', now())";
    PGresult* res = pool_->execParams(sql, {payload.dump()});
    pool_->clear(res);
}

std::vector<PendingEvent> PgPendingEventRepository::claimDue(int limit) {
    // 原子地领取一批事件：锁定到期行，标记为 processing，并
    // 返回它们。FOR UPDATE SKIP LOCKED 使其在多个副本间安全；
    // 单个语句立即提交（自动提交）。
    const std::string sql =
        "WITH claimed AS ("
        "  SELECT id FROM pending_events"
        "  WHERE status = 'pending' AND next_retry_at <= now()"
        "  ORDER BY next_retry_at"
        "  FOR UPDATE SKIP LOCKED"
        "  LIMIT $1"
        ")"
        "UPDATE pending_events SET status = 'processing', claimed_at = now(),"
        "  claimed_by = $2"
        " WHERE id IN (SELECT id FROM claimed)"
        " RETURNING id, payload, retry_count, last_error";

    PGresult* res = pool_->execParams(sql, {std::to_string(limit), claimed_by_});
    std::vector<PendingEvent> out;
    if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK) {
        const int n = PQntuples(res);
        for (int i = 0; i < n; ++i) {
            PendingEvent e;
            e.id = std::stol(PQgetvalue(res, i, 0));
            e.payload =
                nlohmann::json::parse(PQgetvalue(res, i, 1), nullptr, false);
            e.retry_count = std::stoi(PQgetvalue(res, i, 2));
            e.last_error = PQgetvalue(res, i, 3) ? PQgetvalue(res, i, 3) : "";
            out.push_back(std::move(e));
        }
    }
    pool_->clear(res);
    return out;
}

void PgPendingEventRepository::markSuccess(long id) {
    const std::string sql = "DELETE FROM pending_events WHERE id = $1";
    PGresult* res = pool_->execParams(sql, {std::to_string(id)});
    pool_->clear(res);
}

void PgPendingEventRepository::markRetry(long id, int64_t delay_ms,
                                         const std::string& error) {
    // make_interval(secs => double) 接受小数秒。
    const double secs = static_cast<double>(delay_ms) / 1000.0;
    const std::string sql =
        "UPDATE pending_events SET status = 'pending',"
        "  retry_count = retry_count + 1,"
        "  next_retry_at = now() + make_interval(secs => $2::float8),"
        "  last_error = $3"
        " WHERE id = $1";
    PGresult* res = pool_->execParams(
        sql, {std::to_string(id), std::to_string(secs), error});
    pool_->clear(res);
}

void PgPendingEventRepository::markDeadLetter(long id,
                                              const std::string& error) {
    const std::string sql =
        "UPDATE pending_events SET status = 'dead_letter',"
        "  last_error = $2, retry_count = retry_count + 1"
        " WHERE id = $1";
    PGresult* res = pool_->execParams(sql, {std::to_string(id), error});
    pool_->clear(res);
}

DispatchClaim PgPendingEventRepository::tryMarkDispatched(
    const std::string& event_id, const std::string& target_user,
    const std::string& channel, const std::string& notify_id) {
    const std::string sql =
        "INSERT INTO notify_dispatch (event_id, target_user, channel, notify_id)"
        " VALUES ($1, $2, $3, $4)"
        " ON CONFLICT (event_id, target_user, channel) DO NOTHING";
    PGresult* res =
        pool_->execParams(sql, {event_id, target_user, channel, notify_id});
    DispatchClaim claim = DispatchClaim::AlreadyDone;
    if (okStatus(res)) {
        // 插入一行时 PQcmdTuples 返回 "1"，冲突时返回 "0"。
        const char* n = PQcmdTuples(res);
        claim = (n && std::string(n) == "1") ? DispatchClaim::Claimed
                                             : DispatchClaim::AlreadyDone;
    }
    pool_->clear(res);
    return claim;
}

void PgPendingEventRepository::clearDispatched(
    const std::string& event_id, const std::string& target_user,
    const std::string& channel) {
    const std::string sql =
        "DELETE FROM notify_dispatch WHERE event_id = $1 AND target_user = $2"
        " AND channel = $3";
    PGresult* res = pool_->execParams(sql, {event_id, target_user, channel});
    pool_->clear(res);
}

long PgPendingEventRepository::countByStatus(const std::string& status) const {
    const std::string sql =
        "SELECT count(*) FROM pending_events WHERE status = $1";
    PGresult* res = pool_->execParams(sql, {status});
    long n = 0;
    if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK &&
        PQntuples(res) > 0) {
        n = std::stol(PQgetvalue(res, 0, 0));
    }
    pool_->clear(res);
    return n;
}
