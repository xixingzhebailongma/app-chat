#include "db/PgNotifyLogRepository.h"

#include <nlohmann/json.hpp>

#include <utility>

#include "db/PgPool.h"
#include "dto/NotifySendDto.h"

PgNotifyLogRepository::PgNotifyLogRepository(std::shared_ptr<PgPool> pool)
    : pool_(std::move(pool)) {}

void PgNotifyLogRepository::save(const std::string& notify_id,
                                 const NotifySendRequest& req,
                                 const std::vector<ChannelAttempt>& attempts,
                                 const std::string& final_status) {
    // 主表：notify_logs。target_roles 存 JSONB；space_teachers/is_fallback
    // 用布尔参数。7.6 事件属性缺失时（transition 为空、计数为 -1）落 NULL。
    const std::string insertLog =
        "INSERT INTO notify_logs (notify_id, event_id, event_type, space_id,"
        " device_id, device_label, severity, content, target_roles,"
        " space_teachers, final_status, transition, duration_sec, consecutive,"
        " offline_minutes)"
        " VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9::jsonb,$10::boolean,$11,"
        " NULLIF($12,''), NULLIF($13::int,-1), NULLIF($14::int,-1),"
        " NULLIF($15::int,-1))";

    std::vector<PgPool::Stmt> stmts;
    stmts.push_back({insertLog,
                     {notify_id,
                      req.event_id,
                      req.event_type,
                      req.space_id,
                      req.device_id,
                      req.device_label,
                      req.severity,
                      req.content,
                      nlohmann::json(req.target.roles).dump(),
                      req.target.space_teachers ? "true" : "false",
                      final_status,
                      req.transition,
                      std::to_string(req.duration_sec),
                      std::to_string(req.consecutive),
                      std::to_string(req.offline_minutes)}});

    const std::string insertAttempt =
        "INSERT INTO notify_attempts (notify_id, channel, seq, status,"
        " error_msg, is_fallback, target_user, errcode)"
        " VALUES ($1,$2,$3::int,$4,$5,$6::boolean,NULLIF($7,''),"
        " NULLIF($8::int,0))";
    int seq = 1;
    for (const auto& a : attempts) {
        stmts.push_back({insertAttempt,
                         {notify_id,
                          a.channel,
                          std::to_string(seq++),
                          a.success ? "success" : "failed",
                          a.error_msg,
                          a.is_fallback ? "true" : "false",
                          a.target_user,
                          std::to_string(a.errcode)}});
    }

    pool_->execTransaction(stmts);
}
