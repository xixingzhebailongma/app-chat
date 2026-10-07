#include "db/PgUserSpacesRepository.h"

#include <libpq-fe.h>

#include <utility>

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

}  // namespace

PgUserSpacesRepository::PgUserSpacesRepository(std::shared_ptr<PgPool> pool)
    : pool_(std::move(pool)) {}

bool PgUserSpacesRepository::contains(const std::string& user_id,
                                      const std::string& space_id) const {
    const std::string sql =
        "SELECT 1 FROM user_spaces WHERE user_id = $1 AND space_id = $2 LIMIT 1";
    PGresult* res = pool_->execParams(sql, {user_id, space_id});
    bool found = false;
    if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK) {
        found = PQntuples(res) > 0;
    }
    pool_->clear(res);
    return found;
}

std::vector<std::string> PgUserSpacesRepository::spacesForUser(
    const std::string& user_id) const {
    const std::string sql =
        "SELECT space_id FROM user_spaces WHERE user_id = $1 ORDER BY space_id";
    PGresult* res = pool_->execParams(sql, {user_id});
    std::vector<std::string> out;
    if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK) {
        const int n = PQntuples(res);
        out.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            out.emplace_back(str(res, i, 0));
        }
    }
    pool_->clear(res);
    return out;
}

void PgUserSpacesRepository::add(const std::string& user_id,
                                 const std::string& space_id) {
    const std::string sql =
        "INSERT INTO user_spaces (user_id, space_id) VALUES ($1, $2)"
        " ON CONFLICT (user_id, space_id) DO NOTHING";
    PGresult* res = pool_->execParams(sql, {user_id, space_id});
    pool_->clear(res);
}

void PgUserSpacesRepository::remove(const std::string& user_id,
                                    const std::string& space_id) {
    const std::string sql =
        "DELETE FROM user_spaces WHERE user_id = $1 AND space_id = $2";
    PGresult* res = pool_->execParams(sql, {user_id, space_id});
    pool_->clear(res);
}

void PgUserSpacesRepository::removeBySpace(const std::string& space_id) {
    const std::string sql = "DELETE FROM user_spaces WHERE space_id = $1";
    PGresult* res = pool_->execParams(sql, {space_id});
    pool_->clear(res);
}
