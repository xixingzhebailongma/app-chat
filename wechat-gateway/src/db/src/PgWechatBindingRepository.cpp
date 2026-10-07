#include "db/PgWechatBindingRepository.h"

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

PgWechatBindingRepository::PgWechatBindingRepository(
    std::shared_ptr<PgPool> pool)
    : pool_(std::move(pool)) {}

std::optional<WechatBinding> PgWechatBindingRepository::findByOpenid(
    const std::string& channel, const std::string& openid) const {
    const std::string sql =
        "SELECT channel, openid, user_id, role, name FROM wechat_bindings"
        " WHERE channel = $1 AND openid = $2";
    PGresult* res = pool_->execParams(sql, {channel, openid});
    std::optional<WechatBinding> out;
    if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK &&
        PQntuples(res) > 0) {
        WechatBinding b;
        b.channel = str(res, 0, 0);
        b.openid = str(res, 0, 1);
        b.user_id = str(res, 0, 2);
        b.role = str(res, 0, 3);
        b.name = str(res, 0, 4);
        out = std::move(b);
    }
    pool_->clear(res);
    return out;
}

std::optional<WechatBinding> PgWechatBindingRepository::findByUser(
    const std::string& channel, const std::string& user_id) const {
    const std::string sql =
        "SELECT channel, openid, user_id, role, name FROM wechat_bindings"
        " WHERE channel = $1 AND user_id = $2";
    PGresult* res = pool_->execParams(sql, {channel, user_id});
    std::optional<WechatBinding> out;
    if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK &&
        PQntuples(res) > 0) {
        WechatBinding b;
        b.channel = str(res, 0, 0);
        b.openid = str(res, 0, 1);
        b.user_id = str(res, 0, 2);
        b.role = str(res, 0, 3);
        b.name = str(res, 0, 4);
        out = std::move(b);
    }
    pool_->clear(res);
    return out;
}

void PgWechatBindingRepository::save(const WechatBinding& binding) {
    const std::string sql =
        "INSERT INTO wechat_bindings (channel, openid, user_id, role, name)"
        " VALUES ($1, $2, $3, $4, $5)"
        " ON CONFLICT (channel, openid) DO UPDATE SET"
        " user_id = EXCLUDED.user_id, role = EXCLUDED.role,"
        " name = EXCLUDED.name, updated_at = now()";
    PGresult* res = pool_->execParams(
        sql, {binding.channel, binding.openid, binding.user_id, binding.role,
              binding.name});
    pool_->clear(res);
}
