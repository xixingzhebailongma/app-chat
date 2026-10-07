#include "services/AlertService.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "db/OperationLogWriter.h"
#include "utils/Authz.h"

namespace {

// 告警列表项 / 单条详情的统一序列化：非 admin 调用者隐藏
// operator_id / operator_name / remark（脱敏只在服务层完成，字段置空
// 而非不返回，保持结构稳定）。
nlohmann::json alertItemJson(const Alert& alert, bool isAdmin) {
    return nlohmann::json{{"alert_id", alert.alert_id},
                          {"space_id", alert.space_id},
                          {"event_type", alert.event_type},
                          {"status", alert.status},
                          {"operator_id", isAdmin ? alert.operator_id : ""},
                          {"operator_name", isAdmin ? alert.operator_name : ""},
                          {"remark", isAdmin ? alert.remark : ""},
                          {"device_id", alert.device_id},
                          {"device_label", alert.device_label},
                          {"created_at", alert.created_at},
                          {"updated_at", alert.updated_at}};
}

}  // namespace

AlertService::AlertService(std::shared_ptr<AlertRepository> alerts,
                           std::shared_ptr<UserSpacesRepository> userSpaces,
                           std::shared_ptr<OperationLogRepository> opLogRepo,
                           std::string opLogFile)
    : alerts_(std::move(alerts)),
      userSpaces_(std::move(userSpaces)),
      opLogRepo_(std::move(opLogRepo)),
      opLogFile_(std::move(opLogFile)) {}

ServiceResult AlertService::list(const std::string& userId,
                                 const std::string& role,
                                 const std::string& spaceId,
                                 const AlertFilter& filter, int page,
                                 int pageSize) {
    // 空间边界（与 SpaceService::list 一致）：管理员可查看全部告警；教师仅能
    // 查看其已绑定空间的告警；其他角色直接拒绝，而非返回空列表。
    std::vector<std::string> scope;  // 空 = 不过滤（仅 admin 场景）
    if (role == "admin") {
        // scope 保持空 = 全部
    } else if (role == "teacher") {
        scope = userSpaces_->spacesForUser(userId);
        if (scope.empty()) {  // 无绑定：返回空，绝不落到全表
            return ServiceResult::ok(nlohmann::json{
                {"alerts", nlohmann::json::array()},
                {"total", 0},
                {"page", page},
                {"page_size", pageSize},
                {"has_more", false}});
        }
    } else {
        return ServiceResult::error(drogon::k403Forbidden, "forbidden");
    }

    // 单教室过滤：单点校验用 authz::canAccessSpace（admin 恒 true）。
    if (!spaceId.empty()) {
        if (!authz::canAccessSpace(role, userId, spaceId, *userSpaces_)) {
            return ServiceResult::error(drogon::k403Forbidden,
                                        "no access to space");
        }
        scope = {spaceId};
    }

    // 分页护栏（与 AccessRecordService::list 一致）。
    if (page < 1) {
        page = 1;
    }
    if (page > 10000) {
        page = 10000;
    }
    if (pageSize < 1) {
        pageSize = 50;
    }
    if (pageSize > 200) {
        pageSize = 200;
    }
    const int offset = (page - 1) * pageSize;

    const auto rows = alerts_->query(scope, filter, pageSize, offset);
    const int total = alerts_->count(scope, filter);

    nlohmann::json arr = nlohmann::json::array();
    for (const auto& alert : rows) {
        arr.push_back(alertItemJson(alert, role == "admin"));
    }
    return ServiceResult::ok(nlohmann::json{
        {"alerts", arr},
        {"total", total},
        {"page", page},
        {"page_size", pageSize},
        {"has_more", page * pageSize < total}});
}

ServiceResult AlertService::handle(const std::string& alertId,
                                   const std::string& userId,
                                   const std::string& role,
                                   const std::string& remark,
                                   const std::string& status) {
    if (role != "admin") {
        return ServiceResult::error(drogon::k403Forbidden,
                                    "admin role required");
    }

    if (status != "handling" && status != "resolved") {
        return ServiceResult::error(drogon::k400BadRequest,
                                    "status must be handling or resolved");
    }

    // 先取告警：拿 space_id/event_type 写日志，并完成 404 判断（替代原先
    // 依赖 handle() 返回值判 404）。
    const auto alert = alerts_->findById(alertId);
    if (!alert) {
        return ServiceResult::error(drogon::k404NotFound, "alert not found");
    }
    if (!alerts_->handle(alertId, userId, remark, status)) {  // 保留防御性检查
        return ServiceResult::error(drogon::k404NotFound, "alert not found");
    }

    // 运维日志：告警处置写 operation_logs（与 scene_execute 同表，op_type 区分）。
    OperationLogEntry entry;
    entry.op_type = "alert_handle";
    entry.user_id = userId;
    entry.space_id = alert->space_id;   // Pg 侧 NULL 已被 str() 归一为 ""
    entry.scene_id = "";                // NOT NULL 兜底空串
    entry.success_count = 1;
    entry.failed_count = 0;
    entry.detail = nlohmann::json{{"alert_id", alertId},
                                  {"event_type", alert->event_type},
                                  {"status", status},
                                  {"remark", remark}}.dump();
    // 与 SceneService::writeOperationLog 同一入口：DB 成功落库，失败走 JSONL 文件兜底。
    oplog::writeOperationLog(opLogRepo_.get(), entry, opLogFile_);

    return ServiceResult::ok(
        nlohmann::json{{"alert_id", alertId}, {"status", status}});
}

ServiceResult AlertService::timeline(const std::string& alertId,
                                     const std::string& role) {
    if (role != "admin") {
        return ServiceResult::error(drogon::k403Forbidden,
                                    "admin role required");
    }

    const auto alert = alerts_->findById(alertId);
    if (!alert) {
        return ServiceResult::error(drogon::k404NotFound, "alert not found");
    }

    nlohmann::json timelineArr = nlohmann::json::array();
    if (opLogRepo_) {  // 内存模式 opLogRepo 为空 -> 空时间线（预期，非 bug）
        OperationLogFilter f;
        f.op_type = "alert_handle";
        f.alert_id = alertId;
        auto rows = opLogRepo_->query(f, -1, 0);  // 全量（limit<0 不加 LIMIT）
        std::reverse(rows.begin(), rows.end());   // id ASC（旧→新）
        for (const auto& r : rows) {
            auto detail = nlohmann::json::parse(r.detail, nullptr, false);
            const bool isObj = detail.is_object();
            // 注：remark/status 实际恒为字符串（handle 写入必为 string，
            // producer upsert 恒 ""）；即便 detail 中该键为 null，
            // value(key,"") 也只会返回 null——暂不额外兜底。
            timelineArr.push_back(
                {{"id", r.id},
                 {"status", isObj ? detail.value("status", "") : ""},
                 {"user_id", r.user_id},
                 {"operator_name", r.operator_name},
                 {"remark", isObj ? detail.value("remark", "") : ""},
                 {"created_at", r.created_at}});
        }
    }

    return ServiceResult::ok(nlohmann::json{
        {"alert", {{"alert_id", alert->alert_id},
                   {"space_id", alert->space_id},
                   {"event_type", alert->event_type},
                   {"status", alert->status},
                   {"operator_id", alert->operator_id},
                   {"operator_name", alert->operator_name},
                   {"remark", alert->remark},
                   {"device_id", alert->device_id},
                   {"device_label", alert->device_label},
                   {"created_at", alert->created_at},
                   {"updated_at", alert->updated_at}}},
        {"timeline", timelineArr}});
}

ServiceResult AlertService::detail(const std::string& alertId,
                                   const std::string& userId,
                                   const std::string& role) {
    const auto alert = alerts_->findById(alertId);
    if (!alert) {
        return ServiceResult::error(drogon::k404NotFound, "alert not found");
    }

    if (role == "admin") {
        return ServiceResult::ok(
            nlohmann::json{{"alert", alertItemJson(*alert, true)}});
    }
    if (role == "teacher") {
        const auto spaces = userSpaces_->spacesForUser(userId);
        if (std::find(spaces.begin(), spaces.end(), alert->space_id) ==
            spaces.end()) {
            // 越权与不存在统一 404，避免暴露告警是否存在。
            return ServiceResult::error(drogon::k404NotFound, "alert not found");
        }
        return ServiceResult::ok(
            nlohmann::json{{"alert", alertItemJson(*alert, false)}});
    }
    return ServiceResult::error(drogon::k403Forbidden, "forbidden");
}

ServiceResult AlertService::stats(const std::string& userId,
                                  const std::string& role) {
    // 空间边界与 list() 一致：admin 全部；teacher 绑定教室；其他拒绝。
    std::vector<std::string> scope;
    if (role == "admin") {
        // scope 保持空 = 全部
    } else if (role == "teacher") {
        scope = userSpaces_->spacesForUser(userId);
        if (scope.empty()) {  // 无绑定：返回全 0，绝不落到全表
            nlohmann::json byStatus = nlohmann::json::object();
            for (const char* k : {"unhandled", "handling", "resolved"}) {
                byStatus[k] = 0;
            }
            return ServiceResult::ok(nlohmann::json{
                {"by_status", byStatus},
                {"by_event_type", nlohmann::json::object()}});
        }
    } else {
        return ServiceResult::error(drogon::k403Forbidden, "forbidden");
    }

    const AlertStats s = alerts_->stats(scope);

    // by_status：固定三态，缺省补 0。
    nlohmann::json byStatus = nlohmann::json::object();
    for (const char* k : {"unhandled", "handling", "resolved"}) {
        const auto it = s.by_status.find(k);
        byStatus[k] = (it != s.by_status.end()) ? it->second : 0;
    }
    // by_event_type：原始 map（类型开放）。
    nlohmann::json byType = nlohmann::json::object();
    for (const auto& [k, v] : s.by_event_type) {
        byType[k] = v;
    }

    nlohmann::json body{{"by_status", byStatus}, {"by_event_type", byType}};
    if (role == "admin") {  // 仅 admin 返回教室分布
        nlohmann::json bySpace = nlohmann::json::array();
        for (const auto& [sid, c] : s.by_space) {
            bySpace.push_back({{"space_id", sid}, {"count", c}});
        }
        body["by_space"] = bySpace;

        nlohmann::json bySpaceUnhandled = nlohmann::json::array();
        for (const auto& [sid, c] : s.by_space_unhandled) {
            bySpaceUnhandled.push_back({{"space_id", sid}, {"count", c}});
        }
        body["by_space_unhandled"] = bySpaceUnhandled;
    }
    return ServiceResult::ok(body);
}
