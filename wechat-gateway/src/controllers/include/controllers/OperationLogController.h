#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

#include <functional>
#include <memory>

#include "services/OperationLogService.h"

class OperationLogController {
public:
    explicit OperationLogController(std::shared_ptr<OperationLogService> service);

    // GET /api/miniapp/operation-logs
    void list(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    std::shared_ptr<OperationLogService> service_;
};
