#include "controllers/StudentParentSyncController.h"

#include <nlohmann/json.hpp>

#include <string>
#include <utility>
#include <vector>

#include "db/StudentParentRepository.h"
#include "utils/HttpResponseUtil.h"

StudentParentSyncController::StudentParentSyncController(
    std::shared_ptr<StudentParentSyncService> service)
    : service_(std::move(service)) {}

void StudentParentSyncController::sync(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    nlohmann::json body;
    try {
        body = nlohmann::json::parse(req->getBody());
    } catch (const std::exception&) {
        callback(http_util::error(drogon::k400BadRequest, "invalid JSON body"));
        return;
    }

    if (!body.is_object() || !body.contains("parents") ||
        !body["parents"].is_array()) {
        callback(http_util::error(drogon::k400BadRequest,
                                  "parents must be an array"));
        return;
    }

    std::vector<StudentParent> rows;
    for (const auto& p : body["parents"]) {
        if (!p.is_object()) {
            callback(http_util::error(drogon::k400BadRequest,
                                      "each parent must be an object"));
            return;
        }
        StudentParent sp;
        sp.student_no = p.value("student_no", "");
        sp.student_name = p.value("student_name", "");
        sp.parent_phone = p.value("parent_phone", "");
        sp.relation = p.value("relation", std::string("其他"));
        sp.status = p.value("status", std::string("active"));
        if (sp.student_no.empty() || sp.parent_phone.empty()) {
            callback(http_util::error(drogon::k400BadRequest,
                                      "student_no and parent_phone are required"));
            return;
        }
        if (sp.status != "active" && sp.status != "inactive") {
            callback(http_util::error(drogon::k400BadRequest,
                                      "status must be 'active' or 'inactive'"));
            return;
        }
        rows.push_back(std::move(sp));
    }

    const auto r = service_->sync(rows);
    callback(http_util::jsonResponse(r.status, r.body));
}
