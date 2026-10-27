#include "db/PgAiConversationRepository.h"

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

constexpr const char* kTsSelect =
    " to_char(created_at AT TIME ZONE 'UTC',"
    " 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at,"
    " to_char(updated_at AT TIME ZONE 'UTC',"
    " 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS updated_at";

AiConversation parseConv(PGresult* res, int row) {
    AiConversation c;
    c.conversation_id = str(res, row, 0);
    c.user_id = str(res, row, 1);
    c.title = str(res, row, 2);
    c.created_at = str(res, row, 3);
    c.updated_at = str(res, row, 4);
    return c;
}

}  // namespace

PgAiConversationRepository::PgAiConversationRepository(
    std::shared_ptr<PgPool> pool)
    : pool_(std::move(pool)) {}

std::optional<AiConversation> PgAiConversationRepository::findConversation(
    const std::string& conversation_id) const {
    const std::string sql =
        "SELECT conversation_id, user_id, title," + std::string(kTsSelect) +
        " FROM ai_conversations WHERE conversation_id = $1";
    PGresult* res = pool_->execParams(sql, {conversation_id});
    std::optional<AiConversation> out;
    if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK &&
        PQntuples(res) > 0) {
        out = parseConv(res, 0);
    }
    pool_->clear(res);
    return out;
}

std::vector<AiConversation> PgAiConversationRepository::listConversations(
    const std::string& user_id) const {
    // 只列有消息的会话（空会话不展示），按 updated_at 降序。
    const std::string sql =
        "SELECT c.conversation_id, c.user_id, c.title,"
        " to_char(c.created_at AT TIME ZONE 'UTC',"
        " 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at,"
        " to_char(c.updated_at AT TIME ZONE 'UTC',"
        " 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS updated_at"
        " FROM ai_conversations c"
        " WHERE c.user_id = $1"
        "   AND EXISTS (SELECT 1 FROM ai_messages m"
        "               WHERE m.conversation_id = c.conversation_id)"
        " ORDER BY c.updated_at DESC, c.conversation_id DESC";
    PGresult* res = pool_->execParams(sql, {user_id});
    std::vector<AiConversation> out;
    if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK) {
        const int n = PQntuples(res);
        out.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            out.push_back(parseConv(res, i));
        }
    }
    pool_->clear(res);
    return out;
}

std::vector<AiMessage> PgAiConversationRepository::listMessages(
    const std::string& conversation_id, int limit) const {
    std::string sql =
        "SELECT role, content,"
        " to_char(created_at AT TIME ZONE 'UTC',"
        " 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at"
        " FROM ai_messages WHERE conversation_id = $1 ORDER BY id DESC";
    std::vector<std::string> params{conversation_id};
    if (limit > 0) {
        sql += " LIMIT $2";
        params.push_back(std::to_string(limit));
    }
    PGresult* res = pool_->execParams(sql, params);
    std::vector<AiMessage> out;
    if (okStatus(res) && PQresultStatus(res) == PGRES_TUPLES_OK) {
        const int n = PQntuples(res);
        out.reserve(static_cast<size_t>(n));
        // 降序取最近 N 条后再翻转，返回时间正序（旧在前）。
        for (int i = n - 1; i >= 0; --i) {
            AiMessage m;
            m.role = str(res, i, 0);
            m.content = str(res, i, 1);
            m.created_at = str(res, i, 2);
            out.push_back(m);
        }
    }
    pool_->clear(res);
    return out;
}

void PgAiConversationRepository::saveTurn(
    const AiConversation& conv, bool ensureConversation,
    const std::vector<AiMessage>& messages) {
    std::vector<PgPool::Stmt> stmts;
    if (ensureConversation) {
        stmts.push_back(
            {"INSERT INTO ai_conversations (conversation_id, user_id, title)"
             " VALUES ($1, $2, $3) ON CONFLICT (conversation_id) DO NOTHING",
             {conv.conversation_id, conv.user_id, conv.title}});
    }
    for (const auto& m : messages) {
        stmts.push_back(
            {"INSERT INTO ai_messages (conversation_id, role, content)"
             " VALUES ($1, $2, $3)",
             {conv.conversation_id, m.role, m.content}});
    }
    stmts.push_back(
        {"UPDATE ai_conversations SET updated_at = now()"
         " WHERE conversation_id = $1",
         {conv.conversation_id}});
    pool_->execTransaction(stmts);
}

void PgAiConversationRepository::deleteConversation(
    const std::string& conversation_id) {
    const std::string sql =
        "DELETE FROM ai_conversations WHERE conversation_id = $1";
    PGresult* res = pool_->execParams(sql, {conversation_id});
    pool_->clear(res);
}
