#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

#include <functional>
#include <memory>

#include "services/DoorDeviceService.h"

class DoorDeviceController {
public:
    explicit DoorDeviceController(std::shared_ptr<DoorDeviceService> service);

    // GET /api/miniapp/door-devices
    void list(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // POST /api/miniapp/door-devices
    void mark(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // DELETE /api/miniapp/door-devices?space_id=&device_id=
    void unmark(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    std::shared_ptr<DoorDeviceService> service_;
};
