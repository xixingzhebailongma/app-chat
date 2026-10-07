#include "services/AccessRecordService.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "utils/Authz.h"

AccessRecordService::AccessRecordService(
    std::shared_ptr<AccessRecordRepository> records,
    std::shared_ptr<UserSpacesRepository> userSpaces)
    : records_(std::move(records)), userSpaces_(std::move(userSpaces)) {}

ServiceResult AccessRecordService::list(const std::string& userId,
                                        const std::string& role,
                                        const std::string& spaceId,
                                        const std::string& date,
                                        int page, int pageSize,
                                        const std::string& authType) {
    // auth_type 过滤校验（空串 = 不过滤；非法枚举 → 400）。
    if (!authType.empty() && authType != "face" && authType != "card") {
        return ServiceResult::error(drogon::k400BadRequest,
                                    "auth_type must be face or card");
    }
    std::optional<std::string> authFilter;
    if (!authType.empty()) {
        authFilter = authType;
    }

    // 空间范围：teacher 用 spacesForUser 拿集合做 IN 查询；admin 空 = 全部。
    std::vector<std::string> scope;
    if (role == "admin") {
        // scope 保持空 = 不过滤
    } else if (role == "teacher") {
        scope = userSpaces_->spacesForUser(userId);
        if (scope.empty()) {  // 无绑定：返回空，绝不落到全表
            return ServiceResult::ok(nlohmann::json{
                {"records", nlohmann::json::array()},
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

    // 日期 -> 当日 [00:00:00, 23:59:59.999999]（服务端本地日；含微秒上限）。
    std::optional<std::string> from, to;
    if (!date.empty()) {
        from = date + "T00:00:00";
        to = date + "T23:59:59.999999";
    }

    // 分页护栏：page 从 1 起，上限 10000；pageSize 默认 50、上限 200。
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

    const auto rows = records_->query(scope, from, to, pageSize, offset, authFilter);
    const int total = records_->count(scope, from, to, authFilter);

    nlohmann::json arr = nlohmann::json::array();
    for (const auto& r : rows) {
        // 脱敏：不返回 user_id / event_id / 人脸图。
        arr.push_back({{"name", r.name},
                       {"time", r.occurred_at},
                       {"space_id", r.space_id},
                       {"result", r.result},
                       {"auth_type", r.auth_type}});
    }
    return ServiceResult::ok(nlohmann::json{
        {"records", arr},
        {"total", total},
        {"page", page},
        {"page_size", pageSize},
        {"has_more", page * pageSize < total}});
}

ServiceResult AccessRecordService::ingest(const nlohmann::json& body) {
    if (!body.is_object()) {
        return ServiceResult::error(drogon::k400BadRequest, "invalid JSON body");
    }
    if (!body.contains("event_id") || !body["event_id"].is_string() ||
        body["event_id"].get<std::string>().empty()) {
        return ServiceResult::error(drogon::k400BadRequest, "event_id required");
    }
    if (!body.contains("space_id") || !body["space_id"].is_string() ||
        body["space_id"].get<std::string>().empty()) {
        return ServiceResult::error(drogon::k400BadRequest, "space_id required");
    }
    if (!body.contains("occurred_at") || !body["occurred_at"].is_string() ||
        body["occurred_at"].get<std::string>().empty()) {
        return ServiceResult::error(drogon::k400BadRequest,
                                    "occurred_at required");
    }
    const std::string result = body.value("result", "login");
    if (result != "login" && result != "denied") {
        return ServiceResult::error(drogon::k400BadRequest,
                                    "result must be login or denied");
    }
    const std::string authType = body.value("auth_type", "face");
    if (authType != "face" && authType != "card") {
        return ServiceResult::error(drogon::k400BadRequest,
                                    "auth_type must be face or card");
    }

    // user_id 归一化：边侧可能发 int，统一转 string（存储不返回）。
    std::string uid;
    if (body.contains("user_id") && body["user_id"].is_number()) {
        uid = std::to_string(body["user_id"].get<std::int64_t>());
    } else if (body.contains("user_id") && body["user_id"].is_string()) {
        uid = body["user_id"].get<std::string>();
    }

    AccessRecord r;
    r.id = 0;  // Pg BIGSERIAL / InMemory 分配
    r.event_id = body["event_id"].get<std::string>();
    r.space_id = body["space_id"].get<std::string>();
    r.user_id = uid;
    r.name = body.value("name", "");
    r.auth_type = authType;
    r.result = result;
    r.device_id = body.value("device_id", "");
    r.occurred_at = body["occurred_at"].get<std::string>();
    records_->insert(r);
    return ServiceResult::ok(nlohmann::json{{"ok", true}});
}
