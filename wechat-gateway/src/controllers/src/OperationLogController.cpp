#include "controllers/OperationLogController.h"

#include <cstdlib>
#include <string>
#include <utility>

#include "utils/HttpResponseUtil.h"
#include "utils/RequestUtil.h"

namespace {

// 解析非负整数查询参数；空/非法/负数返回 def（分页护栏由服务层再做）。
int parseNonNegativeInt(const std::string& s, int def) {
    if (s.empty()) {
        return def;
    }
    char* end = nullptr;
    const long v = std::strtol(s.c_str(), &end, 10);
    if (end == s.c_str() || *end != '\0' || v < 0) {
        return def;
    }
    return static_cast<int>(v);
}

}  // namespace

OperationLogController::OperationLogController(
    std::shared_ptr<OperationLogService> service)
    : service_(std::move(service)) {}

void OperationLogController::list(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);

    // 只做参数解析与响应封装；权限与脱敏都在 OperationLogService 层完成。
    OperationLogFilter filter;
    filter.op_type = req->getParameter("op_type");
    filter.space_id = req->getParameter("space_id");
    filter.user_id = req->getParameter("user_id");
    filter.from = req->getParameter("from");
    filter.to = req->getParameter("to");
    const int page = parseNonNegativeInt(req->getParameter("page"), 1);
    const int pageSize = parseNonNegativeInt(req->getParameter("page_size"), 50);

    const auto result =
        service_->list(id.user_id, id.role, filter, page, pageSize);
    callback(http_util::jsonResponse(result.status, result.body));
}
