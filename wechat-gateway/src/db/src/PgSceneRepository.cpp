#include "db/PgSceneRepository.h"

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

// 列序：scene_id, space_id, name, kind, device_states。
Scene parseRow(PGresult* res, int row) {
    Scene s;
    s.scene_id = str(res, row, 0);
    s.space_id = str(res, row, 1);
    s.name = str(res, row, 2);
    s.kind = str(res, row, 3);
    s.device_states = str(res, row, 4);
    return s;
}

}  // namespace

PgSceneRepository::PgSceneRepository(std::shared_ptr<PgPool> pool)
    : pool_(std::move(pool)) {}

std::vector<Scene> PgSceneRepository::listBySpace(
    const std::string& space_id) const {
    const std::string sql =
        "SELECT scene_id, space_id, name, kind, device_states FROM scenes"
        " WHERE space_id = $1 ORDER BY created_at, scene_id";
    PGresult* res = pool_->execParams(sql, {space_id});
    std::vector<Scene> out;
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

std::optional<Scene> PgSceneRepository::findById(
    const std::string& scene_id) const {
    const std::string sql =
        "SELECT scene_id, space_id, name, kind, device_states FROM scenes"
        " WHERE scene_id = $1";
    PGresult* res = pool_->execParams(sql, {scene_id});
    std::optional<Scene> out;
    if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK &&
        PQntuples(res) > 0) {
        out = parseRow(res, 0);
    }
    pool_->clear(res);
    return out;
}

void PgSceneRepository::upsert(const Scene& scene) {
    const std::string sql =
        "INSERT INTO scenes (scene_id, space_id, name, kind, device_states)"
        " VALUES ($1, $2, $3, $4, $5)"
        " ON CONFLICT (scene_id) DO UPDATE SET"
        " name = EXCLUDED.name, kind = EXCLUDED.kind,"
        " device_states = EXCLUDED.device_states, updated_at = now()";
    PGresult* res = pool_->execParams(
        sql, {scene.scene_id, scene.space_id, scene.name, scene.kind,
              scene.device_states});
    pool_->clear(res);
}

void PgSceneRepository::remove(const std::string& scene_id) {
    const std::string sql = "DELETE FROM scenes WHERE scene_id = $1";
    PGresult* res = pool_->execParams(sql, {scene_id});
    pool_->clear(res);
}
