#pragma once

#include <drogon/HttpTypes.h>
#include <nlohmann/json.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "clients/GoBackendClient.h"
#include "db/OperationLogRepository.h"
#include "db/SceneRepository.h"
#include "db/SpaceRepository.h"
#include "db/UserSpacesRepository.h"
#include "utils/ErrorBody.h"
#include "utils/ServiceResult.h"

// scene 执行单台设备的下发结果。
struct SceneDeviceResult {
    std::string device_id;
    std::string command;
    int status = 0;  // go-backend 透传的 HTTP 状态；传输失败记 0
};

// 单台下发的目标（custom 场景来自 device_states；all_on/all_off 来自规则匹配）。
struct SceneTarget {
    std::string type;
    std::string id;
    std::string command;
};

// 返回给控制器的结果，用于场景执行。
struct SceneResult {
    drogon::HttpStatusCode status = drogon::k500InternalServerError;
    drogon::ContentType contentType = drogon::CT_APPLICATION_JSON;
    std::string body;
    std::string warning;
    std::vector<SceneDeviceResult> results;

    static SceneResult error(drogon::HttpStatusCode s, const std::string& msg) {
        SceneResult r;
        r.status = s;
        r.body = http_util::errorBodyForStatus(static_cast<int>(s), msg).dump();
        return r;
    }

    static SceneResult errorWithCode(drogon::HttpStatusCode s,
                                     const std::string& code,
                                     const std::string& msg) {
        SceneResult r;
        r.status = s;
        r.body = http_util::errorBody(code, msg).dump();
        return r;
    }

    void buildSummary(const std::string& sceneId, const std::string& spaceId) {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& d : results) {
            arr.push_back({{"device_id", d.device_id},
                           {"command", d.command},
                           {"status", d.status}});
        }
        body = nlohmann::json{{"ok", true},
                              {"scene_id", sceneId},
                              {"space_id", spaceId},
                              {"applied", results.size()},
                              {"warning", warning},
                              {"results", arr}}
                   .dump();
    }
};

// 场景服务：CRUD（同步）+ 执行（异步，展开为 on/off 逐台转发 go-backend）。
// 场景挂空间（按教室共享）。激活态（spaces.active_scene_id）在执行成功后写入、
// 在手动设备控制时清空（见 DeviceControlService）。
class SceneService {
public:
    using Callback = std::function<void(const SceneResult&)>;

    SceneService(std::shared_ptr<GoBackendClient> goBackend,
                 std::shared_ptr<UserSpacesRepository> userSpaces,
                 std::shared_ptr<SpaceRepository> spaceRepo,
                 std::shared_ptr<SceneRepository> sceneRepo,
                 std::shared_ptr<OperationLogRepository> opLogRepo,
                 std::string opLogFile);

    // GET /api/miniapp/scenes?space_id=（懒种默认场景后返回列表 + active_scene_id）。
    ServiceResult list(const std::string& userId, const std::string& role,
                       const std::string& spaceId);

    // POST /api/miniapp/scenes：强制 kind=custom（不接收 kind 入参）。
    ServiceResult create(const std::string& userId, const std::string& role,
                         const std::string& spaceId, const std::string& name,
                         const std::string& deviceStates);

    // PUT /api/miniapp/scenes/{scene_id}：改 name/device_states（kind 不可改）。
    ServiceResult update(const std::string& userId, const std::string& role,
                         const std::string& sceneId,
                         std::optional<std::string> name,
                         std::optional<std::string> deviceStates);

    // DELETE /api/miniapp/scenes/{scene_id}：硬删；若是当前激活场景则一并清空。
    ServiceResult remove(const std::string& userId, const std::string& role,
                         const std::string& sceneId);

    // POST /api/miniapp/scenes/{scene_id}/execute：按 scene.kind 展开执行。
    void execute(const std::string& userId, const std::string& role,
                 const std::string& jwt, const std::string& sceneId,
                 Callback cb);

private:
    // custom 场景的 device_states JSON 解析成下发目标；失败填 err。
    bool parseDeviceStates(const std::string& deviceStates,
                           std::vector<SceneTarget>& out,
                           std::string& err) const;

    // 逐台下发 targets，完成后写激活态 + 日志 + 回调。
    void dispatch(const std::string& userId, const std::string& jwt,
                  const Scene& scene,
                  const std::vector<SceneTarget>& targets, Callback cb);

    void writeOperationLog(const std::string& userId,
                           const std::string& spaceId,
                           const std::string& sceneId,
                           const std::vector<SceneDeviceResult>& results);

    std::shared_ptr<GoBackendClient> goBackend_;
    std::shared_ptr<UserSpacesRepository> userSpaces_;
    std::shared_ptr<SpaceRepository> spaceRepo_;
    std::shared_ptr<SceneRepository> sceneRepo_;
    std::shared_ptr<OperationLogRepository> opLogRepo_;
    std::string opLogFile_;
};
