#include "db/PgNotifyTargetResolver.h"

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

}  // namespace

PgNotifyTargetResolver::PgNotifyTargetResolver(std::shared_ptr<PgPool> pool)
    : pool_(std::move(pool)) {}

TargetResolution PgNotifyTargetResolver::resolve(const std::string& channel,
                                                 const std::string& user_id) {
    const std::string sql =
        "SELECT external_id FROM user_notify_bindings "
        "WHERE channel = $1 AND user_id = $2";
    PGresult* res = pool_->execParams(sql, {channel, user_id});
    TargetResolution out;
    if (!res) {
        out.error = "pg query failed";
        return out;
    }
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        out.error = PQresultErrorMessage(res);
        pool_->clear(res);
        return out;
    }
    if (PQntuples(res) > 0) {
        out.value = std::string(PQgetvalue(res, 0, 0));
    } else {
        out.value = std::nullopt;
    }
    pool_->clear(res);
    return out;
}

bool PgNotifyTargetResolver::save(const NotifyBinding& b) {
    const std::string extra = b.external_extra.empty() ? "{}" : b.external_extra;
    const std::string sql =
        "INSERT INTO user_notify_bindings "
        "    (user_id, channel, external_id, external_extra) "
        "VALUES ($1, $2, $3, $4::jsonb) "
        "ON CONFLICT (channel, user_id) DO UPDATE SET "
        "    external_id = EXCLUDED.external_id, "
        "    external_extra = EXCLUDED.external_extra, "
        "    updated_at = now()";
    PGresult* res =
        pool_->execParams(sql, {b.user_id, b.channel, b.external_id, extra});
    const bool ok = okStatus(res);
    pool_->clear(res);
    return ok;
}

bool PgNotifyTargetResolver::remove(const std::string& channel,
                                    const std::string& user_id) {
    const std::string sql =
        "DELETE FROM user_notify_bindings WHERE channel = $1 AND user_id = $2";
    PGresult* res = pool_->execParams(sql, {channel, user_id});
    const bool ok = okStatus(res);
    pool_->clear(res);
    return ok;
}

std::vector<NotifyBinding> PgNotifyTargetResolver::list(
    const std::string& channel) {
    std::vector<NotifyBinding> out;
    const std::string sql =
        "SELECT user_id, channel, external_id, "
        "       COALESCE(external_extra::text, '{}'), "
        "       updated_at::text "
        "FROM user_notify_bindings WHERE channel = $1 ORDER BY user_id";
    PGresult* res = pool_->execParams(sql, {channel});
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK) {
        const int n = PQntuples(res);
        for (int i = 0; i < n; ++i) {
            NotifyBinding b;
            b.user_id = PQgetvalue(res, i, 0);
            b.channel = PQgetvalue(res, i, 1);
            b.external_id = PQgetvalue(res, i, 2);
            b.external_extra = PQgetvalue(res, i, 3);
            b.updated_at = PQgetvalue(res, i, 4);
            out.push_back(std::move(b));
        }
    }
    pool_->clear(res);
    return out;
}
