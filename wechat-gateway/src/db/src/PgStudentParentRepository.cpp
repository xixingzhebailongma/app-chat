#include "db/PgStudentParentRepository.h"

#include <libpq-fe.h>

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

// 查询「是否存在匹配行」。
bool exists(PgPool& pool, const std::string& sql,
            const std::vector<std::string>& params) {
    PGresult* res = pool.execParams(sql, params);
    bool present = false;
    if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK) {
        present = PQntuples(res) > 0;
    }
    pool.clear(res);
    return present;
}

}  // namespace

PgStudentParentRepository::PgStudentParentRepository(
    std::shared_ptr<PgPool> pool)
    : pool_(std::move(pool)) {}

bool PgStudentParentRepository::hasActiveStudent(
    const std::string& student_no) const {
    return exists(
        *pool_,
        "SELECT 1 FROM student_parents WHERE student_no = $1 AND status ="
        " 'active' LIMIT 1",
        {student_no});
}

bool PgStudentParentRepository::matchesActiveParent(
    const std::string& student_no, const std::string& parent_phone) const {
    return exists(
        *pool_,
        "SELECT 1 FROM student_parents WHERE student_no = $1 AND parent_phone ="
        " $2 AND status = 'active' LIMIT 1",
        {student_no, parent_phone});
}

std::string PgStudentParentRepository::findStudentName(
    const std::string& student_no) const {
    const std::string sql =
        "SELECT student_name FROM student_parents WHERE student_no = $1 AND"
        " status = 'active' ORDER BY id LIMIT 1";
    PGresult* res = pool_->execParams(sql, {student_no});
    std::string name;
    if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK &&
        PQntuples(res) > 0) {
        name = str(res, 0, 0);
    }
    pool_->clear(res);
    return name;
}

std::vector<StudentParent> PgStudentParentRepository::listAll() const {
    const std::string sql =
        "SELECT student_no, student_name, parent_phone, relation, status"
        " FROM student_parents ORDER BY student_no, parent_phone";
    PGresult* res = pool_->execParams(sql, {});
    std::vector<StudentParent> out;
    if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK) {
        const int n = PQntuples(res);
        out.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            StudentParent row;
            row.student_no = str(res, i, 0);
            row.student_name = str(res, i, 1);
            row.parent_phone = str(res, i, 2);
            row.relation = str(res, i, 3);
            row.status = str(res, i, 4);
            out.push_back(std::move(row));
        }
    }
    pool_->clear(res);
    return out;
}

void PgStudentParentRepository::upsert(const StudentParent& row) {
    const std::string sql =
        "INSERT INTO student_parents (student_no, student_name, parent_phone,"
        " relation, status) VALUES ($1, $2, $3, $4, $5)"
        " ON CONFLICT (student_no, parent_phone) DO UPDATE SET"
        " student_name = EXCLUDED.student_name,"
        " relation = EXCLUDED.relation,"
        " status = EXCLUDED.status";
    PGresult* res = pool_->execParams(
        sql, {row.student_no, row.student_name, row.parent_phone, row.relation,
              row.status});
    pool_->clear(res);
}

void PgStudentParentRepository::remove(const std::string& student_no,
                                       const std::string& parent_phone) {
    const std::string sql =
        "DELETE FROM student_parents WHERE student_no = $1 AND parent_phone ="
        " $2";
    PGresult* res = pool_->execParams(sql, {student_no, parent_phone});
    pool_->clear(res);
}
