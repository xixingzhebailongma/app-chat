#include "db/PgOperationLogRepository.h"

#include <libpq-fe.h>

#include <string>
#include <utility>
#include <vector>

#include "db/PgPool.h"

namespace {

bool okStatus(PGresult* res) {
    if (!res) {
        return false;
    }
    const ExecStatusType s = PQresultStatus(res);
    return s == PGRES_TUPLES_OK || s == PGRES_COMMAND_OK;
}

const char* str(PGresult* res, int row, int col) {
    return PQgetisnull(res, row, col) ? "" : PQgetvalue(res, row, col);
}

// 拼接 WHERE 子句并追加参数（占位符 $1..$n 依次编号）。返回 " WHERE 1=1 ..."。
// 列名统一带别名 o.（query 会 LEFT JOIN wechat_bindings；count 也使用别名 o.）。
std::string buildWhere(std::vector<std::string>& params,
                       const OperationLogFilter& f) {
    std::string where = " WHERE 1=1";
    if (!f.op_type.empty()) {
        params.push_back(f.op_type);
        where += " AND o.op_type = $" + std::to_string(params.size());
    }
    if (!f.space_id.empty()) {
        params.push_back(f.space_id);
        where += " AND o.space_id = $" + std::to_string(params.size());
    }
    if (!f.user_id.empty()) {
        params.push_back(f.user_id);
        where += " AND o.user_id = $" + std::to_string(params.size());
    }
    if (!f.alert_id.empty()) {
        params.push_back(f.alert_id);
        where +=
            " AND o.detail->>'alert_id' = $" + std::to_string(params.size());
    }
    if (!f.from.empty()) {
        params.push_back(f.from);
        where += " AND o.created_at >= $" + std::to_string(params.size());
    }
    if (!f.to.empty()) {
        params.push_back(f.to);
        where += " AND o.created_at <= $" + std::to_string(params.size());
    }
    return where;
}

}  // namespace

PgOperationLogRepository::PgOperationLogRepository(std::shared_ptr<PgPool> pool)
    : pool_(std::move(pool)) {}

bool PgOperationLogRepository::insert(const OperationLogEntry& e) {
    const std::string sql =
        "INSERT INTO operation_logs"
        " (op_type, user_id, space_id, scene_id, success_count, failed_count,"
        " detail)"
        " VALUES ($1, $2, $3, $4, $5, $6, $7::jsonb)";
    PGresult* res = pool_->execParams(
        sql, {e.op_type, e.user_id, e.space_id, e.scene_id,
              std::to_string(e.success_count), std::to_string(e.failed_count),
              e.detail});
    const bool ok = res != nullptr && PQresultStatus(res) == PGRES_COMMAND_OK;
    pool_->clear(res);
    return ok;
}

std::vector<OperationLogRow> PgOperationLogRepository::query(
    const OperationLogFilter& f, int limit, int offset) const {
    std::vector<std::string> params;
    std::string sql =
        "SELECT o.id, o.op_type, o.user_id,"
        " COALESCE(w.name, '') AS operator_name,"
        " o.space_id, o.scene_id, o.success_count, o.failed_count,"
        " o.detail::text,"
        " to_char(o.created_at AT TIME ZONE 'UTC',"
        " 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"')"
        " FROM operation_logs o"
        " LEFT JOIN wechat_bindings w"
        "   ON w.user_id = o.user_id AND w.channel = 'miniapp'";
    sql += buildWhere(params, f);
    sql += " ORDER BY o.id DESC";

    if (limit >= 0) {
        params.push_back(std::to_string(limit));
        sql += " LIMIT $" + std::to_string(params.size());
    }
    if (offset > 0) {
        params.push_back(std::to_string(offset));
        sql += " OFFSET $" + std::to_string(params.size());
    }

    PGresult* res = pool_->execParams(sql, params);
    std::vector<OperationLogRow> out;
    if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK) {
        const int n = PQntuples(res);
        out.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            OperationLogRow r;
            r.id = std::stoll(str(res, i, 0));
            r.op_type = str(res, i, 1);
            r.user_id = str(res, i, 2);
            r.operator_name = str(res, i, 3);
            r.space_id = str(res, i, 4);
            r.scene_id = str(res, i, 5);
            r.success_count = std::stoi(str(res, i, 6));
            r.failed_count = std::stoi(str(res, i, 7));
            r.detail = str(res, i, 8);
            r.created_at = str(res, i, 9);
            out.push_back(std::move(r));
        }
    }
    pool_->clear(res);
    return out;
}

int PgOperationLogRepository::count(const OperationLogFilter& f) const {
    std::vector<std::string> params;
    std::string sql = "SELECT COUNT(*) FROM operation_logs o";
    sql += buildWhere(params, f);

    PGresult* res = pool_->execParams(sql, params);
    int n = 0;
    if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK &&
        PQntuples(res) > 0) {
        n = std::stoi(str(res, 0, 0));
    }
    pool_->clear(res);
    return n;
}
