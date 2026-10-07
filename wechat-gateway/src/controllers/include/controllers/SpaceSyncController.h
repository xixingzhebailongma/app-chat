#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

#include <functional>
#include <memory>

#include "services/SpaceSyncService.h"

// POST /internal/spaces/sync（InternalTokenFilter 鉴权）。
// 边侧在空间增删后回灌全量空间列表，网关据此对账本地 spaces 缓存
// 并级联清理 user_spaces / door_devices（设计文档 7.7 收尾项 e）。
// 契约：回灌必须是「全量列表」，否则全量对账会误删未出现于本次回灌的空间。
class SpaceSyncController {
public:
    explicit SpaceSyncController(std::shared_ptr<SpaceSyncService> service);

    void sync(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    std::shared_ptr<SpaceSyncService> service_;
};
