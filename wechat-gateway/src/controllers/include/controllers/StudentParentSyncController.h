#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

#include <functional>
#include <memory>

#include "services/StudentParentSyncService.h"

// POST /internal/student-parents/sync（InternalTokenFilter 鉴权）。
// 学校/运营在花名册变更后回灌全量花名册，网关据此对账本地 student_parents。
// 契约：回灌必须是「全量列表」，否则全量对账会误删未出现于本次回灌的行。
class StudentParentSyncController {
public:
    explicit StudentParentSyncController(
        std::shared_ptr<StudentParentSyncService> service);

    void sync(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    std::shared_ptr<StudentParentSyncService> service_;
};
