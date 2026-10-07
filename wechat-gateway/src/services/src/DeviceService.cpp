#include "services/DeviceService.h"

#include <utility>

#include "utils/Authz.h"

DeviceService::DeviceService(std::shared_ptr<GoBackendClient> goBackend,
                             std::shared_ptr<UserSpacesRepository> spaces)
    : goBackend_(std::move(goBackend)), spaces_(std::move(spaces)) {}

void DeviceService::listDevices(const std::string& userId,
                                const std::string& role,
                                const std::string& jwt,
                                const std::string& spaceId, Callback cb) {
    // 空间边界（设计文档 7.3，与 DeviceControlService::control 一致）：
    // 管理员可查看全部；教师仅其已绑定空间；其他人一律拒绝。
    if (!spaces_ || !authz::canAccessSpace(role, userId, spaceId, *spaces_)) {
        cb(DeviceResult::error(drogon::k403Forbidden,
                               "no permission for this space"));
        return;
    }

    std::string query;
    if (!spaceId.empty()) {
        query = "space_id=" + spaceId;
    }
    goBackend_->get(UpstreamConfig::instance().devicesListPath, query,
                    GoBackendClient::bearer(jwt),
                    [cb = std::move(cb)](const GoBackendResponse& r) {
                        if (!r.ok) {
                            // go-backend 不可达 -> 503 + 机器可读代码
                            // （对齐 DeviceControlService，设计文档 13.7）。
                            cb(DeviceResult::errorWithCode(
                                drogon::k503ServiceUnavailable,
                                "GO_BACKEND_UNAVAILABLE",
                                r.errmsg.empty() ? "go-backend unreachable"
                                                 : r.errmsg));
                            return;
                        }
                        DeviceResult out;
                        out.status = r.status;
                        out.contentType = r.contentType;
                        // go-backend 返回裸设备数组（综合屏 API 约定）；
                        // 将其包装为 {"devices":[...]} 供小程序使用。
                        // 对象响应则原样透传。
                        auto parsed =
                            nlohmann::json::parse(r.body, nullptr, false);
                        if (!parsed.is_discarded() && parsed.is_array()) {
                            out.body =
                                nlohmann::json{{"devices", parsed}}.dump();
                        } else {
                            out.body = r.body;
                        }
                        cb(out);
                    });
}
