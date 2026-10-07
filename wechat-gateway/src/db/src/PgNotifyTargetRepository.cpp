#include "db/PgNotifyTargetRepository.h"

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

std::vector<std::string> collectIds(PGresult* res) {
    std::vector<std::string> out;
    if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK) {
        const int n = PQntuples(res);
        out.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            out.emplace_back(str(res, i, 0));
        }
    }
    return out;
}

}  // namespace

PgNotifyTargetRepository::PgNotifyTargetRepository(std::shared_ptr<PgPool> pool)
    : pool_(std::move(pool)) {}

std::vector<std::string> PgNotifyTargetRepository::usersByRole(
    const std::string& role) const {
    const std::string sql =
        "SELECT user_id FROM user_roles WHERE role = $1 ORDER BY user_id";
    PGresult* res = pool_->execParams(sql, {role});
    std::vector<std::string> out = collectIds(res);
    pool_->clear(res);
    return out;
}

std::vector<std::string> PgNotifyTargetRepository::usersBySpace(
    const std::string& space_id) const {
    const std::string sql =
        "SELECT user_id FROM user_spaces WHERE space_id = $1 ORDER BY user_id";
    PGresult* res = pool_->execParams(sql, {space_id});
    std::vector<std::string> out = collectIds(res);
    pool_->clear(res);
    return out;
}
