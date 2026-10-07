#include "services/DeviceControlService.h"

#include <utility>

#include "utils/Authz.h"

DeviceControlService::DeviceControlService(
    std::shared_ptr<GoBackendClient> goBackend,
    std::shared_ptr<UserSpacesRepository> spaces,
    std::shared_ptr<DoorDeviceRepository> doorRepo)
    : goBackend_(std::move(goBackend)),
      spaces_(std::move(spaces)),
      doorRepo_(std::move(doorRepo)) {}

const std::set<std::string>& DeviceControlService::allowedCommands() {
    static const std::set<std::string> cmds = {"on", "off", "toggle"};
    return cmds;
}

void DeviceControlService::control(const std::string& userId,
                                   const std::string& role,
                                   const std::string& jwt,
                                   const std::string& spaceId,
                                   const std::string& deviceType,
                                   const std::string& deviceId,
                                   const std::string& command,
                                   bool confirm,
                                   Callback cb) {
    // 1. 命令白名单 —— 仅 on/off/toggle（综合屏 API 约定）。
    const auto& allowed = allowedCommands();
    if (allowed.count(command) == 0) {
        cb(DeviceControlResult::error(
            drogon::k400BadRequest,
            "unsupported command; supported: on / off / toggle"));
        return;
    }

    // 2. 门禁白名单确认（7.5③ 开门二次确认）：
    //   - toggle → 400（门禁不支持 toggle，防歧义）
    //   - off（开门/开锁）且 !confirm → 428 CONFIRM_REQUIRED
    //   - on（锁门）→ 直接通过
    // contains 在热路径上（每次控制一次查询）；PG 模式为一次 DB 查询，
    // 如有性能问题改为内存缓存（本次不做）。
    if (doorRepo_ && doorRepo_->contains(spaceId, deviceId)) {
        if (command == "toggle") {
            cb(DeviceControlResult::error(
                drogon::k400BadRequest,
                "door does not support toggle; use on/off"));
            return;
        }
        if (command == "off" && !confirm) {
            cb(DeviceControlResult::error(
                drogon::k428PreconditionRequired,
                "door open requires confirmation"));
            return;
        }
    }

    // 3. 权限检查（设计文档 7.3）：管理员始终允许；教师必须已绑定该空间；
    //    其他人一律拒绝。
    if (!spaces_ ||
        !authz::canAccessSpace(role, userId, spaceId, *spaces_)) {
        cb(DeviceControlResult::error(drogon::k403Forbidden,
                                      "no permission for this space"));
        return;
    }

    // 4. 转发到 go-backend 并原样透传响应。
    const std::string path = UpstreamConfig::fill(
        UpstreamConfig::instance().devicesControlPath,
        {{"device_type", deviceType}, {"device_id", deviceId}});
    const std::string body =
        nlohmann::json{{UpstreamConfig::instance().controlCommandKey, command}}
            .dump();
    goBackend_->post(
        path, body, GoBackendClient::bearer(jwt),
        [cb = std::move(cb)](const GoBackendResponse& r) {
            if (!r.ok) {
                // go-backend 不可达 -> 503 + 机器可读代码
                // （设计文档 13.7）。控制命令不做缓存/重试。
                cb(DeviceControlResult::errorWithCode(
                    drogon::k503ServiceUnavailable, "GO_BACKEND_UNAVAILABLE",
                    r.errmsg.empty() ? "go-backend unreachable" : r.errmsg));
                return;
            }
            DeviceControlResult out;
            out.status = r.status;
            out.body = r.body;
            out.contentType = r.contentType;
            cb(out);
        });
}
