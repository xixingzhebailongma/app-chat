#include "controllers/AccessRecordController.h"

#include <nlohmann/json.hpp>

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

AccessRecordController::AccessRecordController(
    std::shared_ptr<AccessRecordService> service)
    : service_(std::move(service)) {}

void AccessRecordController::list(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);
    const std::string spaceId = req->getParameter("space_id");
    const std::string date = req->getParameter("date");
    const std::string authType = req->getParameter("auth_type");
    const int page = parseNonNegativeInt(req->getParameter("page"), 1);
    const int pageSize = parseNonNegativeInt(req->getParameter("page_size"), 50);

    const auto r = service_->list(id.user_id, id.role, spaceId, date, page,
                                  pageSize, authType);
    callback(http_util::jsonResponse(r.status, r.body));
}

void AccessRecordController::ingest(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    nlohmann::json body;
    if (!req->getBody().empty()) {
        try {
            body = nlohmann::json::parse(req->getBody());
        } catch (const std::exception&) {
            callback(http_util::error(drogon::k400BadRequest,
                                      "invalid JSON body"));
            return;
        }
    }

    const auto r = service_->ingest(body);
    callback(http_util::jsonResponse(r.status, r.body));
}
