#include "db/PgAlertRepository.h"

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
// 列名统一带别名 a.（query 会 LEFT JOIN wechat_bindings，避免 created_at 歧义；
// count 也使用别名 a. 以复用本函数）。
std::string buildWhere(std::vector<std::string>& params,
                       const std::vector<std::string>& space_ids,
                       const AlertFilter& f) {
    std::string where = " WHERE 1=1";
    if (!space_ids.empty()) {
        where += " AND a.space_id IN (";
        for (size_t i = 0; i < space_ids.size(); ++i) {
            if (i > 0) {
                where += ", ";
            }
            params.push_back(space_ids[i]);
            where += "$" + std::to_string(params.size());
        }
        where += ")";
    }
    if (!f.event_type.empty()) {
        params.push_back(f.event_type);
        where += " AND a.event_type = $" + std::to_string(params.size());
    }
    if (!f.status.empty()) {
        params.push_back(f.status);
        where += " AND a.status = $" + std::to_string(params.size());
    }
    if (!f.from.empty()) {
        params.push_back(f.from);
        where += " AND a.created_at >= $" + std::to_string(params.size());
    }
    if (!f.to.empty()) {
        params.push_back(f.to);
        where += " AND a.created_at <= $" + std::to_string(params.size());
    }
    return where;
}

// 只按空间过滤（stats 用）：space_ids 为空返回 " WHERE 1=1"（不过滤，仅 admin）。
std::string buildSpaceWhere(std::vector<std::string>& params,
                            const std::vector<std::string>& space_ids) {
    std::string where = " WHERE 1=1";
    if (!space_ids.empty()) {
        where += " AND a.space_id IN (";
        for (size_t i = 0; i < space_ids.size(); ++i) {
            if (i > 0) {
                where += ", ";
            }
            params.push_back(space_ids[i]);
            where += "$" + std::to_string(params.size());
        }
        where += ")";
    }
    return where;
}

}  // namespace

PgAlertRepository::PgAlertRepository(std::shared_ptr<PgPool> pool)
    : pool_(std::move(pool)) {}

std::vector<Alert> PgAlertRepository::query(
    const std::vector<std::string>& space_ids, const AlertFilter& f, int limit,
    int offset) const {
    std::vector<std::string> params;
    std::string sql =
        "SELECT a.alert_id, a.space_id, a.event_type, a.status, a.operator_id,"
        " a.remark, a.device_id, a.device_label,"
        " to_char(a.created_at AT TIME ZONE 'UTC',"
        " 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at,"
        " to_char(a.updated_at AT TIME ZONE 'UTC',"
        " 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS updated_at,"
        " COALESCE(w.name, '') AS operator_name"
        " FROM alert_handles a"
        " LEFT JOIN wechat_bindings w"
        "   ON w.user_id = a.operator_id AND w.channel = 'miniapp'";
    sql += buildWhere(params, space_ids, f);
    sql += " ORDER BY a.created_at DESC, a.alert_id DESC";

    if (limit >= 0) {
        params.push_back(std::to_string(limit));
        sql += " LIMIT $" + std::to_string(params.size());
    }
    if (offset > 0) {
        params.push_back(std::to_string(offset));
        sql += " OFFSET $" + std::to_string(params.size());
    }

    PGresult* res = pool_->execParams(sql, params);
    std::vector<Alert> out;
    if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK) {
        const int n = PQntuples(res);
        out.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            Alert a;
            a.alert_id = str(res, i, 0);
            a.space_id = str(res, i, 1);
            a.event_type = str(res, i, 2);
            a.status = str(res, i, 3);
            a.operator_id = str(res, i, 4);
            a.remark = str(res, i, 5);
            a.device_id = str(res, i, 6);
            a.device_label = str(res, i, 7);
            a.created_at = str(res, i, 8);
            a.updated_at = str(res, i, 9);
            a.operator_name = str(res, i, 10);
            out.push_back(std::move(a));
        }
    }
    pool_->clear(res);
    return out;
}

int PgAlertRepository::count(const std::vector<std::string>& space_ids,
                             const AlertFilter& f) const {
    std::vector<std::string> params;
    std::string sql = "SELECT COUNT(*) FROM alert_handles a";
    sql += buildWhere(params, space_ids, f);

    PGresult* res = pool_->execParams(sql, params);
    int n = 0;
    if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK &&
        PQntuples(res) > 0) {
        n = std::stoi(str(res, 0, 0));
    }
    pool_->clear(res);
    return n;
}

bool PgAlertRepository::upsert(const Alert& alert) {
    const std::string sql =
        "INSERT INTO alert_handles (alert_id, space_id, event_type, status,"
        " operator_id, remark, device_id, device_label)"
        " VALUES ($1, $2, $3, $4, $5, $6, $7, $8)"
        " ON CONFLICT (alert_id) DO NOTHING";
    PGresult* res = pool_->execParams(
        sql, {alert.alert_id, alert.space_id, alert.event_type, alert.status,
              alert.operator_id, alert.remark, alert.device_id,
              alert.device_label});
    // 成功与否只看 DB 命令是否执行成功；重复 event_id 被 DO NOTHING 跳过
    // 不算失败（幂等，不覆盖管理员已做的处理）。
    const bool ok = okStatus(res) && PQresultStatus(res) == PGRES_COMMAND_OK;
    pool_->clear(res);
    return ok;
}

bool PgAlertRepository::handle(const std::string& alert_id,
                               const std::string& operator_id,
                               const std::string& remark,
                               const std::string& status) {
    const std::string sql =
        "UPDATE alert_handles SET status = $2, operator_id = $3, remark = $4,"
        " updated_at = now() WHERE alert_id = $1";
    PGresult* res =
        pool_->execParams(sql, {alert_id, status, operator_id, remark});
    bool hit = false;
    if (okStatus(res) && PQresultStatus(res) == PGRES_COMMAND_OK) {
        const char* n = PQcmdTuples(res);
        hit = n && std::string(n) == "1";
    }
    pool_->clear(res);
    return hit;
}

std::optional<Alert> PgAlertRepository::findById(
    const std::string& alert_id) const {
    const std::string sql =
        "SELECT a.alert_id, a.space_id, a.event_type, a.status, a.operator_id,"
        " a.remark, a.device_id, a.device_label,"
        " to_char(a.created_at AT TIME ZONE 'UTC',"
        " 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at,"
        " to_char(a.updated_at AT TIME ZONE 'UTC',"
        " 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS updated_at,"
        " COALESCE(w.name, '') AS operator_name"
        " FROM alert_handles a"
        " LEFT JOIN wechat_bindings w"
        "   ON w.user_id = a.operator_id AND w.channel = 'miniapp'"
        " WHERE a.alert_id = $1";
    PGresult* res = pool_->execParams(sql, {alert_id});
    std::optional<Alert> out;
    if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK &&
        PQntuples(res) > 0) {
        Alert a;
        a.alert_id = str(res, 0, 0);
        a.space_id = str(res, 0, 1);
        a.event_type = str(res, 0, 2);
        a.status = str(res, 0, 3);
        a.operator_id = str(res, 0, 4);
        a.remark = str(res, 0, 5);
        a.device_id = str(res, 0, 6);
        a.device_label = str(res, 0, 7);
        a.created_at = str(res, 0, 8);
        a.updated_at = str(res, 0, 9);
        a.operator_name = str(res, 0, 10);
        out = std::move(a);
    }
    pool_->clear(res);
    return out;
}

AlertStats PgAlertRepository::stats(
    const std::vector<std::string>& space_ids) const {
    AlertStats s;

    // 状态分布（status 有 CHECK 约束，仅 unhandled/handling/resolved）。
    {
        std::vector<std::string> params;
        std::string sql = "SELECT a.status, COUNT(*) FROM alert_handles a";
        sql += buildSpaceWhere(params, space_ids);
        sql += " GROUP BY a.status";
        PGresult* res = pool_->execParams(sql, params);
        if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK) {
            for (int i = 0; i < PQntuples(res); ++i) {
                s.by_status[str(res, i, 0)] = std::stoi(str(res, i, 1));
            }
        }
        pool_->clear(res);
    }

    // 类型分布。
    {
        std::vector<std::string> params;
        std::string sql = "SELECT a.event_type, COUNT(*) FROM alert_handles a";
        sql += buildSpaceWhere(params, space_ids);
        sql += " GROUP BY a.event_type";
        PGresult* res = pool_->execParams(sql, params);
        if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK) {
            for (int i = 0; i < PQntuples(res); ++i) {
                s.by_event_type[str(res, i, 0)] = std::stoi(str(res, i, 1));
            }
        }
        pool_->clear(res);
    }

    // 教室分布。
    {
        std::vector<std::string> params;
        std::string sql = "SELECT a.space_id, COUNT(*) FROM alert_handles a";
        sql += buildSpaceWhere(params, space_ids);
        sql += " GROUP BY a.space_id";
        PGresult* res = pool_->execParams(sql, params);
        if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK) {
            for (int i = 0; i < PQntuples(res); ++i) {
                s.by_space[str(res, i, 0)] = std::stoi(str(res, i, 1));
            }
        }
        pool_->clear(res);
    }

    // 未处理告警按教室分布（7.5① 高亮有告警教室用）。
    {
        std::vector<std::string> params;
        std::string sql = "SELECT a.space_id, COUNT(*) FROM alert_handles a";
        std::string where = " WHERE a.status = 'unhandled'";
        if (!space_ids.empty()) {
            where += " AND a.space_id IN (";
            for (size_t i = 0; i < space_ids.size(); ++i) {
                if (i > 0) {
                    where += ", ";
                }
                params.push_back(space_ids[i]);
                where += "$" + std::to_string(params.size());
            }
            where += ")";
        }
        sql += where + " GROUP BY a.space_id";
        PGresult* res = pool_->execParams(sql, params);
        if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK) {
            for (int i = 0; i < PQntuples(res); ++i) {
                s.by_space_unhandled[str(res, i, 0)] =
                    std::stoi(str(res, i, 1));
            }
        }
        pool_->clear(res);
    }

    return s;
}
