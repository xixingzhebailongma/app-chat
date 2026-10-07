#include "controllers/SpaceController.h"

#include <utility>

#include "utils/HttpResponseUtil.h"
#include "utils/RequestUtil.h"

SpaceController::SpaceController(std::shared_ptr<SpaceService> service)
    : service_(std::move(service)) {}

void SpaceController::spaces(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto id = request_util::identity(req);
    const auto result = service_->list(id.user_id, id.role);
    callback(http_util::jsonResponse(result.status, result.body));
}
