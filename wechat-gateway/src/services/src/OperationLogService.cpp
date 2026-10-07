#include "services/OperationLogService.h"

#include <nlohmann/json.hpp>

#include <string>
#include <utility>

OperationLogService::OperationLogService(
    std::shared_ptr<OperationLogRepository> repo)
    : repo_(std::move(repo)) {}

ServiceResult OperationLogService::list(const std::string& /*userId*/,
                                        const std::string& role,
                                        const OperationLogFilter& filter,
                                        int page, int pageSize) {
    if (role != "admin") {
        return ServiceResult::error(drogon::k403Forbidden,
                                    "admin role required");
    }

    // 分页护栏同 AlertService::list（page 1..10000 / pageSize 1..200）。
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

    if (!repo_) {  // 内存模式（无 PG）：无可查询 store，返回空列表
        return ServiceResult::ok(nlohmann::json{
            {"logs", nlohmann::json::array()},
            {"total", 0},
            {"page", page},
            {"page_size", pageSize},
            {"has_more", false}});
    }
    const auto rows = repo_->query(filter, pageSize, offset);
    const int total = repo_->count(filter);

    nlohmann::json arr = nlohmann::json::array();
    for (const auto& r : rows) {
        // detail 是 JSONB 文本；解析失败则原样作为字符串返回，不吞字段。
        auto detail = nlohmann::json::parse(r.detail, nullptr, false);
        if (detail.is_discarded()) {
            detail = r.detail;  // 非 JSON（异常数据）→ 原样字符串
        }
        arr.push_back({{"id", r.id},
                       {"op_type", r.op_type},
                       {"user_id", r.user_id},
                       {"operator_name", r.operator_name},
                       {"space_id", r.space_id},
                       {"scene_id", r.scene_id},
                       {"success_count", r.success_count},
                       {"failed_count", r.failed_count},
                       {"detail", detail},
                       {"created_at", r.created_at}});
    }
    return ServiceResult::ok(nlohmann::json{{"logs", arr},
                                            {"total", total},
                                            {"page", page},
                                            {"page_size", pageSize},
                                            {"has_more", page * pageSize < total}});
}
