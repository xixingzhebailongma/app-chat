#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

#include <functional>
#include <memory>

#include "services/AccessRecordService.h"

class AccessRecordController {
public:
    explicit AccessRecordController(std::shared_ptr<AccessRecordService> service);

    // GET /api/miniapp/access-records?space_id=&date=&auth_type=&page=&page_size=
    void list(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    // POST /internal/access-records（边侧摄取，InternalTokenFilter 鉴权）。
    void ingest(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    std::shared_ptr<AccessRecordService> service_;
};
