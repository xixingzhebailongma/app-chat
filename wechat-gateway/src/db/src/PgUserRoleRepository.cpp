#include "db/PgUserRoleRepository.h"

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

PgUserRoleRepository::PgUserRoleRepository(std::shared_ptr<PgPool> pool)
    : pool_(std::move(pool)) {}

void PgUserRoleRepository::setRoles(const std::string& user_id,
                                    const std::vector<std::string>& roles) {
    std::vector<PgPool::Stmt> stmts;
    stmts.push_back({"DELETE FROM user_roles WHERE user_id = $1", {user_id}});
    stmts.reserve(1 + roles.size());
    for (const auto& role : roles) {
        stmts.push_back(
            {"INSERT INTO user_roles (user_id, role) VALUES ($1, $2)",
             {user_id, role}});
    }
    pool_->execTransaction(stmts);
}

void PgUserRoleRepository::removeUser(const std::string& user_id) {
    const std::string sql = "DELETE FROM user_roles WHERE user_id = $1";
    auto res = pool_->execParams(sql, {user_id});
    pool_->clear(res);
}

std::vector<std::string> PgUserRoleRepository::usersByRole(
    const std::string& role) const {
    const std::string sql =
        "SELECT user_id FROM user_roles WHERE role = $1 ORDER BY user_id";
    PGresult* res = pool_->execParams(sql, {role});
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

std::string PgUserRoleRepository::roleOf(const std::string& user_id) const {
    const std::string sql = "SELECT role FROM user_roles WHERE user_id = $1";
    PGresult* res = pool_->execParams(sql, {user_id});
    std::vector<std::string> roles;
    if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK) {
        const int n = PQntuples(res);
        roles.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            roles.emplace_back(str(res, i, 0));
        }
    }
    pool_->clear(res);
    return highestRole(roles);
}
