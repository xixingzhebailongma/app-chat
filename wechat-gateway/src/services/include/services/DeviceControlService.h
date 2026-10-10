#pragma once

#include <drogon/HttpTypes.h>
#include <nlohmann/json.hpp>

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "clients/GoBackendClient.h"
#include "db/DoorDeviceRepository.h"
#include "db/SpaceRepository.h"
#include "db/UserSpacesRepository.h"
#include "utils/ErrorBody.h"

// 返回给控制器的结果，用于 POST /api/miniapp/device/control。
// `body` 承载本地生成的 {"error":{...}} 结构，或原样透传的 go-backend
// 响应主体。
struct DeviceControlResult {
    drogon::HttpStatusCode status = drogon::k500InternalServerError;
    std::string body;
    drogon::ContentType contentType = drogon::CT_APPLICATION_JSON;

    static DeviceControlResult error(drogon::HttpStatusCode s,
                                     const std::string& msg) {
        DeviceControlResult r;
        r.status = s;
        r.body = http_util::errorBodyForStatus(static_cast<int>(s), msg).dump();
        return r;
    }

    // 带显式机器可读字符串代码的错误（设计文档 13.7）。
    // 用于上游不可用的失败场景，使前端可以依据该代码（如
    // "GO_BACKEND_UNAVAILABLE"）而非通用的状态码映射来判断。
    static DeviceControlResult errorWithCode(drogon::HttpStatusCode s,
                                             const std::string& code,
                                             const std::string& msg) {
        DeviceControlResult r;
        r.status = s;
        r.body = http_util::errorBody(code, msg).dump();
        return r;
    }
};

// 实现设计文档 八 中的控制命令转发流程：
// JWT 身份由调用方提供（已由 JwtFilter 校验）；本服务负责角色/空间授权、
// 高风险 confirm 门槛以及 go-backend 转发。
class DeviceControlService {
public:
    using Callback = std::function<void(const DeviceControlResult&)>;

    DeviceControlService(std::shared_ptr<GoBackendClient> goBackend,
                         std::shared_ptr<UserSpacesRepository> spaces,
                         std::shared_ptr<DoorDeviceRepository> doorRepo,
                         std::shared_ptr<SpaceRepository> spaceRepo);

    void control(const std::string& userId,
                 const std::string& role,
                 const std::string& jwt,
                 const std::string& spaceId,
                 const std::string& deviceType,
                 const std::string& deviceId,
                 const std::string& command,
                 bool confirm,
                 Callback cb);

    // 命令白名单（设计文档 八 命令白名单）：on / off / toggle。小程序从不做
    // 连续调节（温度、亮度等）；场景式控制由 scene/execute 展开为 on/off
    // 命令。为匹配综合屏 API 约定（仅文档化 on/off/toggle），高风险命令
    // （开门/重启/强制）已移除。
    static const std::set<std::string>& allowedCommands();

private:
    std::shared_ptr<GoBackendClient> goBackend_;
    std::shared_ptr<UserSpacesRepository> spaces_;
    std::shared_ptr<DoorDeviceRepository> doorRepo_;  // 可空（内存模式若未注入）
    std::shared_ptr<SpaceRepository> spaceRepo_;      // 手动控制后清空激活场景
};
