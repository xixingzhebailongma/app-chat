#pragma once

#include <drogon/HttpTypes.h>
#include <nlohmann/json.hpp>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "clients/GoBackendClient.h"
#include "db/OperationLogRepository.h"
#include "db/UserSpacesRepository.h"
#include "utils/ErrorBody.h"

// scene/execute 单台设备的下发结果。
struct SceneDeviceResult {
    std::string device_id;
    std::string command;
    int status = 0;  // go-backend 透传的 HTTP 状态；传输失败记 0
};

// 返回给控制器的结果，用于 POST /api/miniapp/scene/execute。
// body 承载 buildSummary() 生成的聚合摘要，或本地生成的 {"error":{...}}。
struct SceneResult {
    drogon::HttpStatusCode status = drogon::k500InternalServerError;
    drogon::ContentType contentType = drogon::CT_APPLICATION_JSON;
    std::string body;
    std::string warning;                     // applied==0（或全部下发失败）时非空
    std::vector<SceneDeviceResult> results;  // 每台设备的下发结果

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

    // 把 ok/scene_id/space_id/applied/warning/results 序列化进 body。
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

// POST /api/miniapp/scene/execute（设计文档 7.4②）：把预设场景展开为多条
// on/off，逐台转发 go-backend /devices/{type}/{id}/cmd，聚合结果并写审计日志。
class SceneService {
public:
    using Callback = std::function<void(const SceneResult&)>;

    SceneService(std::shared_ptr<GoBackendClient> goBackend,
                 std::shared_ptr<UserSpacesRepository> spaces,
                 std::shared_ptr<OperationLogRepository> opLogRepo,
                 std::string opLogFile);

    void execute(const std::string& userId, const std::string& role,
                 const std::string& jwt, const std::string& spaceId,
                 const std::string& sceneId, Callback cb);

private:
    // 写审计日志；DB 不可用/写入失败时降级追加本地 JSONL 文件。
    void writeOperationLog(const std::string& userId,
                           const std::string& spaceId,
                           const std::string& sceneId,
                           const std::vector<SceneDeviceResult>& results);

    std::shared_ptr<GoBackendClient> goBackend_;
    std::shared_ptr<UserSpacesRepository> spaces_;
    std::shared_ptr<OperationLogRepository> opLogRepo_;
    std::string opLogFile_;
};
