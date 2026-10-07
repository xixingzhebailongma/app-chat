#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

#include <functional>
#include <memory>

#include "services/DeviceService.h"

class DeviceController {
public:
    explicit DeviceController(std::shared_ptr<DeviceService> service);

    // GET /api/miniapp/devices —— 空间边界在 DeviceService 内部强制执行
    // （设计文档 7.3）；此控制器仅原样转发。
    void devices(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    std::shared_ptr<DeviceService> service_;
};
