#include "db/PgSubscriptionRepository.h"

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

}  // namespace

PgSubscriptionRepository::PgSubscriptionRepository(
    std::shared_ptr<PgPool> pool)
    : pool_(std::move(pool)) {}

void PgSubscriptionRepository::grant(const std::string& user_id,
                                     const std::string& template_id,
                                     bool long_term) {
    // one_time：首次插 1、冲突 +1（长期 -1 不被覆盖）；long_term：恒 -1。
    const std::string sql =
        "INSERT INTO subscriptions (user_id, template_id, quota) "
        "VALUES ($1, $2, CASE WHEN $3::boolean THEN -1 ELSE 1 END) "
        "ON CONFLICT (user_id, template_id) DO UPDATE SET quota = "
        "  CASE WHEN $3::boolean THEN -1 "
        "       WHEN subscriptions.quota < 0 THEN subscriptions.quota "
        "       ELSE subscriptions.quota + 1 END, updated_at = now()";
    PGresult* res = pool_->execParams(
        sql, {user_id, template_id, long_term ? "true" : "false"});
    pool_->clear(res);
}

bool PgSubscriptionRepository::reserve(const std::string& user_id,
                                       const std::string& template_id) {
    // 原子预扣：quota > 0 扣 1；-1 不扣但返回行；0/无行不返回行。
    const std::string sql =
        "UPDATE subscriptions "
        "SET quota = CASE WHEN quota > 0 THEN quota - 1 ELSE quota END, "
        "    updated_at = now() "
        "WHERE user_id = $1 AND template_id = $2 AND quota <> 0 "
        "RETURNING quota";
    PGresult* res = pool_->execParams(sql, {user_id, template_id});
    const bool ok = okStatus(res) &&
                    PQresultStatus(res) == PGRES_TUPLES_OK &&
                    PQntuples(res) > 0;
    pool_->clear(res);
    return ok;
}

void PgSubscriptionRepository::refill(const std::string& user_id,
                                      const std::string& template_id) {
    // 失败回补：仅对 one_time（quota >= 0）回补；长期 -1 不补。
    const std::string sql =
        "UPDATE subscriptions SET quota = quota + 1, updated_at = now() "
        "WHERE user_id = $1 AND template_id = $2 AND quota >= 0";
    PGresult* res = pool_->execParams(sql, {user_id, template_id});
    pool_->clear(res);
}

void PgSubscriptionRepository::revoke(const std::string& user_id,
                                      const std::string& template_id) {
    const std::string sql =
        "UPDATE subscriptions SET quota = 0, updated_at = now() "
        "WHERE user_id = $1 AND template_id = $2";
    PGresult* res = pool_->execParams(sql, {user_id, template_id});
    pool_->clear(res);
}

void PgSubscriptionRepository::revokeAll(const std::string& user_id) {
    const std::string sql = "DELETE FROM subscriptions WHERE user_id = $1";
    PGresult* res = pool_->execParams(sql, {user_id});
    pool_->clear(res);
}

int PgSubscriptionRepository::quotaOf(const std::string& user_id,
                                      const std::string& template_id) const {
    const std::string sql =
        "SELECT quota FROM subscriptions WHERE user_id = $1 AND template_id = $2";
    PGresult* res = pool_->execParams(sql, {user_id, template_id});
    int quota = 0;
    if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK &&
        PQntuples(res) > 0) {
        quota = std::atoi(PQgetvalue(res, 0, 0));
    }
    pool_->clear(res);
    return quota;
}
