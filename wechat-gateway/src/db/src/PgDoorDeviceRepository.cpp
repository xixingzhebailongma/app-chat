#include "db/PgDoorDeviceRepository.h"

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

}  // namespace

PgDoorDeviceRepository::PgDoorDeviceRepository(std::shared_ptr<PgPool> pool)
    : pool_(std::move(pool)) {}

bool PgDoorDeviceRepository::contains(const std::string& space_id,
                                      const std::string& device_id) const {
    const std::string sql =
        "SELECT 1 FROM door_devices WHERE space_id = $1 AND device_id = $2";
    PGresult* res = pool_->execParams(sql, {space_id, device_id});
    const bool hit = okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK &&
                     PQntuples(res) > 0;
    pool_->clear(res);
    return hit;
}

std::vector<DoorDevice> PgDoorDeviceRepository::list(
    const std::string& space_id) const {
    std::string sql =
        "SELECT space_id, device_id, label, marked_by,"
        " to_char(marked_at AT TIME ZONE 'UTC',"
        " 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"')"
        " FROM door_devices";
    std::vector<std::string> params;
    if (!space_id.empty()) {
        params.push_back(space_id);
        sql += " WHERE space_id = $1";
    }
    sql += " ORDER BY space_id, device_id";

    PGresult* res = pool_->execParams(sql, params);
    std::vector<DoorDevice> out;
    if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK) {
        const int n = PQntuples(res);
        out.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            DoorDevice d;
            d.space_id = str(res, i, 0);
            d.device_id = str(res, i, 1);
            d.label = str(res, i, 2);
            d.marked_by = str(res, i, 3);
            d.marked_at = str(res, i, 4);
            out.push_back(std::move(d));
        }
    }
    pool_->clear(res);
    return out;
}

bool PgDoorDeviceRepository::add(const DoorDevice& d) {
    const std::string sql =
        "INSERT INTO door_devices"
        " (space_id, device_id, label, marked_by, marked_at)"
        " VALUES ($1, $2, $3, $4, now())"
        " ON CONFLICT (space_id, device_id) DO UPDATE SET"
        " label = EXCLUDED.label, marked_by = EXCLUDED.marked_by,"
        " marked_at = now()";
    PGresult* res =
        pool_->execParams(sql, {d.space_id, d.device_id, d.label, d.marked_by});
    const bool ok = okStatus(res) && PQresultStatus(res) == PGRES_COMMAND_OK;
    pool_->clear(res);
    return ok;
}

bool PgDoorDeviceRepository::remove(const std::string& space_id,
                                    const std::string& device_id) {
    const std::string sql =
        "DELETE FROM door_devices WHERE space_id = $1 AND device_id = $2";
    PGresult* res = pool_->execParams(sql, {space_id, device_id});
    const bool ok = okStatus(res) && PQresultStatus(res) == PGRES_COMMAND_OK;
    pool_->clear(res);
    return ok;
}

bool PgDoorDeviceRepository::removeBySpace(const std::string& space_id) {
    const std::string sql = "DELETE FROM door_devices WHERE space_id = $1";
    PGresult* res = pool_->execParams(sql, {space_id});
    const bool ok = okStatus(res) && PQresultStatus(res) == PGRES_COMMAND_OK;
    pool_->clear(res);
    return ok;
}
