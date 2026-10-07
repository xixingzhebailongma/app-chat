#include "controllers/DeviceController.h"

#include <utility>

#include "utils/HttpResponseUtil.h"
#include "utils/RequestUtil.h"

DeviceController::DeviceController(std::shared_ptr<DeviceService> service)
    : service_(std::move(service)) {}

void DeviceController::devices(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);
    const std::string jwt = request_util::bearerToken(req);
    const std::string spaceId = req->getParameter("space_id");

    service_->listDevices(
        id.user_id, id.role, jwt, spaceId,
        [callback = std::move(callback)](const DeviceResult& r) {
            callback(http_util::passthrough(r.status, r.contentType, r.body));
        });
}
