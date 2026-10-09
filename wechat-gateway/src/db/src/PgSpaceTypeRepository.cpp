#include "db/PgSpaceTypeRepository.h"

#include <libpq-fe.h>

#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "db/PgPool.h"

namespace {

// 当语句返回元组（SELECT）或命令标签（INSERT/UPDATE/DELETE）时视为成功。
bool okStatus(PGresult* res) {
    if (!res) {
        return false;
    }
    const ExecStatusType s = PQresultStatus(res);
    return s == PGRES_TUPLES_OK || s == PGRES_COMMAND_OK;
}

// 读取可能为 NULL 的文本列；NULL 一律返回空串。
const char* str(PGresult* res, int row, int col) {
    return PQgetisnull(res, row, col) ? "" : PQgetvalue(res, row, col);
}

// PostgreSQL boolean 文本输出为 't' / 'f'。
bool asBool(PGresult* res, int row, int col) {
    return str(res, row, col)[0] == 't';
}

}  // namespace

PgSpaceTypeRepository::PgSpaceTypeRepository(std::shared_ptr<PgPool> pool)
    : pool_(std::move(pool)) {}

std::vector<SpaceType> PgSpaceTypeRepository::listAll() const {
    const std::string sql =
        "SELECT code, name, sort_order, enabled, icon FROM space_types"
        " ORDER BY sort_order, code";
    PGresult* res = pool_->execParams(sql, {});
    std::vector<SpaceType> out;
    if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK) {
        const int n = PQntuples(res);
        out.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            SpaceType t;
            t.code = str(res, i, 0);
            t.name = str(res, i, 1);
            t.sort_order = std::atoi(str(res, i, 2));
            t.enabled = asBool(res, i, 3);
            t.icon = str(res, i, 4);
            out.push_back(std::move(t));
        }
    }
    pool_->clear(res);
    return out;
}

std::optional<SpaceType> PgSpaceTypeRepository::findByCode(
    const std::string& code) const {
    const std::string sql =
        "SELECT code, name, sort_order, enabled, icon FROM space_types"
        " WHERE code = $1";
    PGresult* res = pool_->execParams(sql, {code});
    std::optional<SpaceType> out;
    if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK &&
        PQntuples(res) > 0) {
        SpaceType t;
        t.code = str(res, 0, 0);
        t.name = str(res, 0, 1);
        t.sort_order = std::atoi(str(res, 0, 2));
        t.enabled = asBool(res, 0, 3);
        t.icon = str(res, 0, 4);
        out = std::move(t);
    }
    pool_->clear(res);
    return out;
}

void PgSpaceTypeRepository::upsert(const SpaceType& t) {
    const std::string sql =
        "INSERT INTO space_types (code, name, sort_order, enabled, icon)"
        " VALUES ($1, $2, $3, $4, $5)"
        " ON CONFLICT (code) DO UPDATE SET"
        " name = EXCLUDED.name, sort_order = EXCLUDED.sort_order,"
        " enabled = EXCLUDED.enabled, icon = EXCLUDED.icon,"
        " updated_at = now()";
    PGresult* res = pool_->execParams(
        sql, {t.code, t.name, std::to_string(t.sort_order),
              t.enabled ? "true" : "false", t.icon});
    pool_->clear(res);
}

bool PgSpaceTypeRepository::disable(const std::string& code) {
    const std::string sql =
        "UPDATE space_types SET enabled = false, updated_at = now()"
        " WHERE code = $1";
    PGresult* res = pool_->execParams(sql, {code});
    const bool ok = okStatus(res) && PQresultStatus(res) == PGRES_COMMAND_OK;
    pool_->clear(res);
    return ok;
}

void PgSpaceTypeRepository::setSortOrder(
    const std::vector<std::string>& codes) {
    std::vector<PgPool::Stmt> stmts;
    stmts.reserve(codes.size());
    for (size_t i = 0; i < codes.size(); ++i) {
        stmts.push_back({"UPDATE space_types SET sort_order = $1,"
                         " updated_at = now() WHERE code = $2",
                         {std::to_string(i), codes[i]}});
    }
    pool_->execTransaction(stmts);
}

int PgSpaceTypeRepository::maxSortOrder() const {
    const std::string sql =
        "SELECT COALESCE(MAX(sort_order), -1) FROM space_types";
    PGresult* res = pool_->execParams(sql, {});
    int m = -1;
    if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK &&
        PQntuples(res) > 0) {
        m = std::atoi(str(res, 0, 0));
    }
    pool_->clear(res);
    return m;
}
