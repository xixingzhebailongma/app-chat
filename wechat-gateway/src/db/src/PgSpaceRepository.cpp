#include "db/PgSpaceRepository.h"

#include <libpq-fe.h>

#include <utility>

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

}  // namespace

PgSpaceRepository::PgSpaceRepository(std::shared_ptr<PgPool> pool)
    : pool_(std::move(pool)) {}

std::vector<Space> PgSpaceRepository::listAll() const {
    const std::string sql = "SELECT space_id, name, type FROM spaces ORDER BY name";
    PGresult* res = pool_->execParams(sql, {});
    std::vector<Space> out;
    if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK) {
        const int n = PQntuples(res);
        out.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            Space s;
            s.space_id = str(res, i, 0);
            s.name = str(res, i, 1);
            s.type = str(res, i, 2);
            out.push_back(std::move(s));
        }
    }
    pool_->clear(res);
    return out;
}

std::optional<Space> PgSpaceRepository::findById(
    const std::string& space_id) const {
    const std::string sql =
        "SELECT space_id, name, type FROM spaces WHERE space_id = $1";
    PGresult* res = pool_->execParams(sql, {space_id});
    std::optional<Space> out;
    if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK &&
        PQntuples(res) > 0) {
        Space s;
        s.space_id = str(res, 0, 0);
        s.name = str(res, 0, 1);
        s.type = str(res, 0, 2);
        out = std::move(s);
    }
    pool_->clear(res);
    return out;
}

void PgSpaceRepository::upsert(const Space& space) {
    const std::string sql =
        "INSERT INTO spaces (space_id, name, type) VALUES ($1, $2, $3)"
        " ON CONFLICT (space_id) DO UPDATE SET name = EXCLUDED.name,"
        " type = EXCLUDED.type";
    PGresult* res =
        pool_->execParams(sql, {space.space_id, space.name, space.type});
    pool_->clear(res);
}

void PgSpaceRepository::remove(const std::string& space_id) {
    const std::string sql = "DELETE FROM spaces WHERE space_id = $1";
    PGresult* res = pool_->execParams(sql, {space_id});
    pool_->clear(res);
}
