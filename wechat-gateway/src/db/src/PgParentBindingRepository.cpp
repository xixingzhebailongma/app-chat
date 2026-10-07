#include "db/PgParentBindingRepository.h"

#include <libpq-fe.h>

#include <cstdlib>
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

PgParentBindingRepository::PgParentBindingRepository(std::shared_ptr<PgPool> pool)
    : pool_(std::move(pool)) {}

std::vector<std::string> PgParentBindingRepository::findParentOpenidsByStudentNo(
    const std::string& student_no) const {
    const std::string sql =
        "SELECT parent_openid_oa FROM parent_student_bindings"
        " WHERE student_no = $1 AND reach_status = 'active'"
        " ORDER BY parent_openid_oa";
    PGresult* res = pool_->execParams(sql, {student_no});
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

std::vector<ParentBinding> PgParentBindingRepository::findBindingsByOpenid(
    const std::string& parent_openid_oa) const {
    const std::string sql =
        "SELECT student_no, parent_openid_oa, phone,"
        " to_char(verified_at, 'YYYY-MM-DD\"T\"HH24:MI:SS.MS\"Z\"')"
        " FROM parent_student_bindings WHERE parent_openid_oa = $1"
        " ORDER BY verified_at";
    PGresult* res = pool_->execParams(sql, {parent_openid_oa});
    std::vector<ParentBinding> out;
    if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK) {
        const int n = PQntuples(res);
        out.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            ParentBinding b;
            b.student_no = str(res, i, 0);
            b.parent_openid_oa = str(res, i, 1);
            b.phone = str(res, i, 2);
            b.verified_at = str(res, i, 3);
            out.push_back(std::move(b));
        }
    }
    pool_->clear(res);
    return out;
}

void PgParentBindingRepository::save(const ParentBinding& binding) {
    // 幂等：同一 (student_no, parent_openid_oa) 已存在则跳过；重绑视为重新
    // 订阅，重置可达性为 active。
    const std::string sql =
        "INSERT INTO parent_student_bindings (student_no, parent_openid_oa,"
        " phone) VALUES ($1, $2, $3)"
        " ON CONFLICT (student_no, parent_openid_oa)"
        " DO UPDATE SET phone = EXCLUDED.phone,"
        "               reach_status = 'active',"
        "               last_unreachable_at = NULL";
    PGresult* res = pool_->execParams(
        sql, {binding.student_no, binding.parent_openid_oa, binding.phone});
    pool_->clear(res);
}

bool PgParentBindingRepository::remove(const std::string& parent_openid_oa,
                                       const std::string& student_no) {
    const std::string sql =
        "DELETE FROM parent_student_bindings WHERE parent_openid_oa = $1"
        " AND student_no = $2";
    PGresult* res = pool_->execParams(sql, {parent_openid_oa, student_no});
    bool removed = false;
    if (okStatus(res) && PQresultStatus(res) == PGRES_COMMAND_OK) {
        const char* affected = PQcmdTuples(res);
        removed = affected && affected[0] != '\0' && std::atoi(affected) > 0;
    }
    pool_->clear(res);
    return removed;
}

void PgParentBindingRepository::updateReachStatus(
    const std::string& parent_openid_oa, const std::string& student_no,
    const std::string& status) {
    const std::string sql =
        "UPDATE parent_student_bindings"
        " SET reach_status = $1, last_unreachable_at = now()"
        " WHERE parent_openid_oa = $2 AND student_no = $3";
    PGresult* res =
        pool_->execParams(sql, {status, parent_openid_oa, student_no});
    pool_->clear(res);
}
