#include "db/PgAccessRecordRepository.h"

#include <libpq-fe.h>

#include <cstdlib>
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
std::string buildWhere(std::vector<std::string>& params,
                       const std::vector<std::string>& space_ids,
                       const std::optional<std::string>& from,
                       const std::optional<std::string>& to,
                       const std::optional<std::string>& authType) {
    std::string where = " WHERE 1=1";
    if (!space_ids.empty()) {
        where += " AND space_id IN (";
        for (size_t i = 0; i < space_ids.size(); ++i) {
            if (i > 0) {
                where += ", ";
            }
            params.push_back(space_ids[i]);
            where += "$" + std::to_string(params.size());
        }
        where += ")";
    }
    if (from) {
        params.push_back(*from);
        where += " AND occurred_at >= $" + std::to_string(params.size());
    }
    if (to) {
        params.push_back(*to);
        where += " AND occurred_at <= $" + std::to_string(params.size());
    }
    if (authType) {
        params.push_back(*authType);
        where += " AND auth_type = $" + std::to_string(params.size());
    }
    return where;
}

}  // namespace

PgAccessRecordRepository::PgAccessRecordRepository(std::shared_ptr<PgPool> pool)
    : pool_(std::move(pool)) {}

void PgAccessRecordRepository::insert(const AccessRecord& r) {
    const std::string sql =
        "INSERT INTO access_records (event_id, space_id, user_id, name,"
        " auth_type, result, device_id, occurred_at)"
        " VALUES ($1, $2, $3, $4, $5, $6, $7, $8)"
        " ON CONFLICT (event_id) DO NOTHING";
    PGresult* res = pool_->execParams(
        sql, {r.event_id, r.space_id, r.user_id, r.name, r.auth_type, r.result,
              r.device_id, r.occurred_at});
    pool_->clear(res);
}

std::vector<AccessRecord> PgAccessRecordRepository::query(
    const std::vector<std::string>& space_ids,
    const std::optional<std::string>& from,
    const std::optional<std::string>& to, int limit, int offset,
    const std::optional<std::string>& authType) const {
    std::vector<std::string> params;
    std::string sql =
        "SELECT id, event_id, space_id, user_id, name, auth_type, result,"
        " device_id, occurred_at FROM access_records";
    sql += buildWhere(params, space_ids, from, to, authType);

    params.push_back(std::to_string(limit));
    const std::string limitPh = "$" + std::to_string(params.size());
    params.push_back(std::to_string(offset));
    const std::string offsetPh = "$" + std::to_string(params.size());
    sql += " ORDER BY occurred_at DESC, id DESC LIMIT " + limitPh + " OFFSET " +
           offsetPh;

    PGresult* res = pool_->execParams(sql, params);
    std::vector<AccessRecord> out;
    if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK) {
        const int n = PQntuples(res);
        out.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            AccessRecord r;
            r.id = std::stoll(str(res, i, 0));
            r.event_id = str(res, i, 1);
            r.space_id = str(res, i, 2);
            r.user_id = str(res, i, 3);
            r.name = str(res, i, 4);
            r.auth_type = str(res, i, 5);
            r.result = str(res, i, 6);
            r.device_id = str(res, i, 7);
            r.occurred_at = str(res, i, 8);
            out.push_back(std::move(r));
        }
    }
    pool_->clear(res);
    return out;
}

int PgAccessRecordRepository::count(
    const std::vector<std::string>& space_ids,
    const std::optional<std::string>& from,
    const std::optional<std::string>& to,
    const std::optional<std::string>& authType) const {
    std::vector<std::string> params;
    std::string sql = "SELECT COUNT(*) FROM access_records";
    sql += buildWhere(params, space_ids, from, to, authType);

    PGresult* res = pool_->execParams(sql, params);
    int n = 0;
    if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK &&
        PQntuples(res) > 0) {
        n = std::stoi(str(res, 0, 0));
    }
    pool_->clear(res);
    return n;
}
