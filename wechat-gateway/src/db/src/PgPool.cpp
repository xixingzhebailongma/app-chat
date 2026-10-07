#include "db/PgPool.h"

#include <libpq-fe.h>

#include <utility>

PgPool::PgPool(std::string conninfo) : conninfo_(std::move(conninfo)) {}

PgPool::~PgPool() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (conn_) {
        PQfinish(conn_);
        conn_ = nullptr;
    }
}

bool PgPool::connectLocked() {
    conn_ = PQconnectdb(conninfo_.c_str());
    return conn_ != nullptr && PQstatus(conn_) == CONNECTION_OK;
}

struct pg_result* PgPool::exec(const std::string& sql) {
    return execParams(sql, {});
}

struct pg_result* PgPool::execParams(const std::string& sql,
                                     const std::vector<std::string>& params) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!conn_ && !connectLocked()) {
        return nullptr;
    }
    std::vector<const char*> values;
    values.reserve(params.size());
    for (const auto& p : params) {
        values.push_back(p.c_str());
    }
    return PQexecParams(conn_, sql.c_str(), static_cast<int>(params.size()),
                        nullptr, values.data(), nullptr, nullptr, 0);
}

void PgPool::clear(struct pg_result* res) {
    if (res) {
        PQclear(res);
    }
}

bool PgPool::execTransaction(const std::vector<Stmt>& stmts) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!conn_ && !connectLocked()) {
        return false;
    }

    auto run = [&](const std::string& sql,
                   const std::vector<std::string>& params) -> PGresult* {
        std::vector<const char*> values;
        values.reserve(params.size());
        for (const auto& p : params) {
            values.push_back(p.c_str());
        }
        return PQexecParams(conn_, sql.c_str(), static_cast<int>(params.size()),
                            nullptr, values.data(), nullptr, nullptr, 0);
    };

    if (PGresult* begin = run("BEGIN", {}); !begin) {
        return false;
    } else {
        PQclear(begin);
    }

    for (const auto& stmt : stmts) {
        PGresult* res = run(stmt.sql, stmt.params);
        if (!res) {
            PGresult* rb = run("ROLLBACK", {});
            if (rb) {
                PQclear(rb);
            }
            return false;
        }
        const ExecStatusType s = PQresultStatus(res);
        const bool ok = s == PGRES_TUPLES_OK || s == PGRES_COMMAND_OK;
        PQclear(res);
        if (!ok) {
            PGresult* rb = run("ROLLBACK", {});
            if (rb) {
                PQclear(rb);
            }
            return false;
        }
    }

    PGresult* commit = run("COMMIT", {});
    if (!commit) {
        PGresult* rb = run("ROLLBACK", {});
        if (rb) {
            PQclear(rb);
        }
        return false;
    }
    PQclear(commit);
    return true;
}
