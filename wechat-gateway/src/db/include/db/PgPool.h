#pragma once

#include <mutex>
#include <string>
#include <utility>
#include <vector>

// 前置声明（libpq）；真正的 include 保留在 .cpp 中，使此
// 头文件保持轻量，且即使未安装 libpq-dev 也能被包含。
struct pg_conn;
struct pg_result;

// 最小化的 libpq 门面（风格：RedisClient）。单连接由互斥锁保护——
// 重试工作线程只有一个线程，因此一个连接足够。调用方通过
// exec/execParams 一次执行一条语句，并用 clear() 释放结果。
// 仅在定义 HAS_LIBPQ 时编译（见 CMakeLists.txt）。
class PgPool {
public:
    explicit PgPool(std::string conninfo);
    ~PgPool();

    PgPool(const PgPool&) = delete;
    PgPool& operator=(const PgPool&) = delete;

    // 执行无参数的语句。连接错误时返回 nullptr；
    // 否则返回结果（调用方必须 clear()）。通过 PQresultStatus 区分
    // 元组（SELECT ... RETURNING）与命令标签（INSERT/UPDATE/DELETE）。
    struct pg_result* exec(const std::string& sql);

    // 执行带 $1..$n 文本参数的语句（PQexecParams —— 防注入安全）。
    // 连接错误时返回 nullptr。
    struct pg_result* execParams(const std::string& sql,
                                 const std::vector<std::string>& params);

    void clear(struct pg_result* res);

    // 一条事务内待执行的语句（$1..$n 参数化，防注入）。
    struct Stmt {
        std::string sql;
        std::vector<std::string> params;
    };

    // 在同一连接、同一把锁内跑完一个事务：BEGIN；逐条执行 stmts；
    // 全部成功 COMMIT，任一失败 ROLLBACK。返回是否全部成功。
    // 供需要原子写多张表（notify_logs + notify_attempts、
    // user_roles 的 DELETE+INSERT）的仓库使用。
    bool execTransaction(const std::vector<Stmt>& stmts);

private:
    bool connectLocked();

    std::string conninfo_;
    struct pg_conn* conn_ = nullptr;
    std::mutex mutex_;
};
