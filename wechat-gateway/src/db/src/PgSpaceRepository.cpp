#include "db/PgSpaceRepository.h"

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

// PostgreSQL boolean 文本输出为 't' / 'f'。
bool asBool(PGresult* res, int row, int col) {
    return str(res, row, col)[0] == 't';
}

// 列序：space_id, name, type, enabled, source, active_scene_id, scenes_seeded。
Space parseRow(PGresult* res, int row) {
    Space s;
    s.space_id = str(res, row, 0);
    s.name = str(res, row, 1);
    s.type = str(res, row, 2);
    s.enabled = asBool(res, row, 3);
    s.source = str(res, row, 4);
    s.active_scene_id = str(res, row, 5);
    s.scenes_seeded = asBool(res, row, 6);
    return s;
}

}  // namespace

PgSpaceRepository::PgSpaceRepository(std::shared_ptr<PgPool> pool)
    : pool_(std::move(pool)) {}

std::vector<Space> PgSpaceRepository::listAll() const {
    const std::string sql =
        "SELECT space_id, name, type, enabled, source FROM spaces ORDER BY name";
    PGresult* res = pool_->execParams(sql, {});
    std::vector<Space> out;
    if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK) {
        const int n = PQntuples(res);
        out.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            out.push_back(parseRow(res, i));
        }
    }
    pool_->clear(res);
    return out;
}

std::vector<Space> PgSpaceRepository::listEnabled() const {
    const std::string sql =
        "SELECT space_id, name, type, enabled, source, active_scene_id, scenes_seeded FROM spaces"
        " WHERE enabled = true ORDER BY name";
    PGresult* res = pool_->execParams(sql, {});
    std::vector<Space> out;
    if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK) {
        const int n = PQntuples(res);
        out.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            out.push_back(parseRow(res, i));
        }
    }
    pool_->clear(res);
    return out;
}

std::optional<Space> PgSpaceRepository::findById(
    const std::string& space_id) const {
    const std::string sql =
        "SELECT space_id, name, type, enabled, source, active_scene_id, scenes_seeded FROM spaces"
        " WHERE space_id = $1";
    PGresult* res = pool_->execParams(sql, {space_id});
    std::optional<Space> out;
    if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK &&
        PQntuples(res) > 0) {
        out = parseRow(res, 0);
    }
    pool_->clear(res);
    return out;
}

void PgSpaceRepository::upsert(const Space& space) {
    const std::string sql =
        "INSERT INTO spaces (space_id, name, type, enabled, source)"
        " VALUES ($1, $2, $3, $4, $5)"
        " ON CONFLICT (space_id) DO UPDATE SET name = EXCLUDED.name,"
        " type = EXCLUDED.type, enabled = EXCLUDED.enabled,"
        " source = EXCLUDED.source";
    PGresult* res = pool_->execParams(
        sql, {space.space_id, space.name, space.type,
              space.enabled ? "true" : "false", space.source});
    pool_->clear(res);
}

void PgSpaceRepository::updateMeta(const std::string& space_id,
                                   const std::string& name,
                                   const std::string& type) {
    const std::string sql =
        "UPDATE spaces SET name = $2, type = $3, source = 'admin'"
        " WHERE space_id = $1";
    PGresult* res = pool_->execParams(sql, {space_id, name, type});
    pool_->clear(res);
}

void PgSpaceRepository::setEnabled(const std::string& space_id, bool enabled) {
    const std::string sql =
        "UPDATE spaces SET enabled = $2, source = 'admin' WHERE space_id = $1";
    PGresult* res = pool_->execParams(sql, {space_id, enabled ? "true" : "false"});
    pool_->clear(res);
}

void PgSpaceRepository::setActiveScene(const std::string& space_id,
                                       const std::string& scene_id) {
    const std::string sql =
        "UPDATE spaces SET active_scene_id = $2 WHERE space_id = $1";
    PGresult* res = pool_->execParams(sql, {space_id, scene_id});
    pool_->clear(res);
}

void PgSpaceRepository::clearActiveScene(const std::string& space_id) {
    const std::string sql =
        "UPDATE spaces SET active_scene_id = '' WHERE space_id = $1";
    PGresult* res = pool_->execParams(sql, {space_id});
    pool_->clear(res);
}

void PgSpaceRepository::markScenesSeeded(const std::string& space_id) {
    const std::string sql =
        "UPDATE spaces SET scenes_seeded = true WHERE space_id = $1";
    PGresult* res = pool_->execParams(sql, {space_id});
    pool_->clear(res);
}

void PgSpaceRepository::remove(const std::string& space_id) {
    const std::string sql = "DELETE FROM spaces WHERE space_id = $1";
    PGresult* res = pool_->execParams(sql, {space_id});
    pool_->clear(res);
}
