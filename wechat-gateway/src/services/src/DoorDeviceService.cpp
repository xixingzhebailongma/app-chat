#include "services/DoorDeviceService.h"

#include <nlohmann/json.hpp>

#include <string>
#include <utility>

#include "db/OperationLogWriter.h"
#include "utils/Authz.h"

DoorDeviceService::DoorDeviceService(
    std::shared_ptr<DoorDeviceRepository> repo,
    std::shared_ptr<UserSpacesRepository> userSpaces,
    std::shared_ptr<OperationLogRepository> opLogRepo, std::string opLogFile)
    : repo_(std::move(repo)),
      userSpaces_(std::move(userSpaces)),
      opLogRepo_(std::move(opLogRepo)),
      opLogFile_(std::move(opLogFile)) {}

ServiceResult DoorDeviceService::list(const std::string& userId,
                                      const std::string& role,
                                      const std::string& spaceId) {
    std::string scopeSpaceId;
    if (role == "admin") {
        scopeSpaceId = spaceId;  // 空 = 全部
    } else if (role == "teacher") {
        if (spaceId.empty()) {
            // 空 scope = 空集，不泄露全量（与 AlertService/stats 一致）。
            return ServiceResult::ok(
                nlohmann::json{{"doors", nlohmann::json::array()}});
        }
        if (!authz::canAccessSpace(role, userId, spaceId, *userSpaces_)) {
            return ServiceResult::error(drogon::k403Forbidden,
                                        "no access to space");
        }
        scopeSpaceId = spaceId;
    } else {
        return ServiceResult::error(drogon::k403Forbidden, "forbidden");
    }

    const auto doors = repo_->list(scopeSpaceId);
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& d : doors) {
        arr.push_back({{"space_id", d.space_id},
                       {"device_id", d.device_id},
                       {"label", d.label},
                       {"marked_by", d.marked_by},
                       {"marked_at", d.marked_at}});
    }
    return ServiceResult::ok(nlohmann::json{{"doors", arr}});
}

ServiceResult DoorDeviceService::mark(const std::string& userId,
                                      const std::string& role,
                                      const std::string& deviceId,
                                      const std::string& spaceId,
                                      const std::string& label) {
    if (role != "admin") {
        return ServiceResult::error(drogon::k403Forbidden,
                                    "admin role required");
    }
    // 基础校验：device_id/space_id 非空（不查 go-backend，信任前端从设备列表选 +
    // 记 marked_by 审计）。不强制 ZB_ 前缀——真实边侧是 ZB_0x{4hex}、mock 是
    // dev_...，强校验会破坏开发/测试。
    if (deviceId.empty() || spaceId.empty()) {
        return ServiceResult::error(drogon::k400BadRequest,
                                    "device_id and space_id are required");
    }

    DoorDevice d;
    d.space_id = spaceId;
    d.device_id = deviceId;
    d.label = label;
    d.marked_by = userId;
    repo_->add(d);  // upsert 覆盖

    OperationLogEntry entry;
    entry.op_type = "door_mark";
    entry.user_id = userId;
    entry.space_id = spaceId;
    entry.scene_id = "";
    entry.success_count = 1;
    entry.failed_count = 0;
    entry.detail =
        nlohmann::json{{"device_id", deviceId}, {"label", label}}.dump();
    oplog::writeOperationLog(opLogRepo_.get(), entry, opLogFile_);

    return ServiceResult::ok(nlohmann::json{{"ok", true}});
}

ServiceResult DoorDeviceService::unmark(const std::string& userId,
                                        const std::string& role,
                                        const std::string& spaceId,
                                        const std::string& deviceId) {
    if (role != "admin") {
        return ServiceResult::error(drogon::k403Forbidden,
                                    "admin role required");
    }
    if (spaceId.empty() || deviceId.empty()) {
        return ServiceResult::error(drogon::k400BadRequest,
                                    "space_id and device_id are required");
    }
    repo_->remove(spaceId, deviceId);  // 幂等

    OperationLogEntry entry;
    entry.op_type = "door_unmark";
    entry.user_id = userId;
    entry.space_id = spaceId;
    entry.scene_id = "";
    entry.success_count = 1;
    entry.failed_count = 0;
    entry.detail = nlohmann::json{{"device_id", deviceId}}.dump();
    oplog::writeOperationLog(opLogRepo_.get(), entry, opLogFile_);

    return ServiceResult::ok(nlohmann::json{{"ok", true}});
}
