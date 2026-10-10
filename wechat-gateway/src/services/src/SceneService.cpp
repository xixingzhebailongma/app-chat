#include "services/SceneService.h"

#include <drogon/drogon.h>

#include <algorithm>
#include <utility>

#include "db/OperationLogWriter.h"
#include "services/SceneDefinitions.h"
#include "utils/Authz.h"
#include "utils/HttpResponseUtil.h"

namespace {

// scene 序列化为 JSON（device_states 反序列化为数组返回给前端）。
nlohmann::json sceneToJson(const Scene& s) {
    nlohmann::json ds = nlohmann::json::parse(s.device_states, nullptr, false);
    if (ds.is_discarded() || !ds.is_array()) {
        ds = nlohmann::json::array();
    }
    return {{"scene_id", s.scene_id},
            {"space_id", s.space_id},
            {"name", s.name},
            {"kind", s.kind},
            {"device_states", ds}};
}

}  // namespace

SceneService::SceneService(std::shared_ptr<GoBackendClient> goBackend,
                           std::shared_ptr<UserSpacesRepository> userSpaces,
                           std::shared_ptr<SpaceRepository> spaceRepo,
                           std::shared_ptr<SceneRepository> sceneRepo,
                           std::shared_ptr<OperationLogRepository> opLogRepo,
                           std::string opLogFile)
    : goBackend_(std::move(goBackend)),
      userSpaces_(std::move(userSpaces)),
      spaceRepo_(std::move(spaceRepo)),
      sceneRepo_(std::move(sceneRepo)),
      opLogRepo_(std::move(opLogRepo)),
      opLogFile_(std::move(opLogFile)) {}

ServiceResult SceneService::list(const std::string& userId,
                                 const std::string& role,
                                 const std::string& spaceId) {
    if (!userSpaces_ ||
        !authz::canAccessSpace(role, userId, spaceId, *userSpaces_)) {
        return ServiceResult::error(drogon::k403Forbidden,
                                    "no permission for this space");
    }
    const auto space = spaceRepo_->findById(spaceId);
    if (!space) {
        return ServiceResult::error(drogon::k404NotFound, "space not found");
    }

    // 懒种默认场景（开启/离开），仅首次。
    if (!space->scenes_seeded) {
        sceneRepo_->upsert(
            Scene{http_util::newId("scn"), spaceId, "开启模式", "all_on", "[]"});
        sceneRepo_->upsert(
            Scene{http_util::newId("scn"), spaceId, "离开模式", "all_off", "[]"});
        spaceRepo_->markScenesSeeded(spaceId);
    }

    nlohmann::json arr = nlohmann::json::array();
    for (const auto& s : sceneRepo_->listBySpace(spaceId)) {
        arr.push_back(sceneToJson(s));
    }
    return ServiceResult::ok(nlohmann::json{
        {"scenes", arr}, {"active_scene_id", space->active_scene_id}});
}

ServiceResult SceneService::create(const std::string& userId,
                                   const std::string& role,
                                   const std::string& spaceId,
                                   const std::string& name,
                                   const std::string& deviceStates) {
    if (!userSpaces_ ||
        !authz::canAccessSpace(role, userId, spaceId, *userSpaces_)) {
        return ServiceResult::error(drogon::k403Forbidden,
                                    "no permission for this space");
    }
    if (name.empty()) {
        return ServiceResult::error(drogon::k400BadRequest,
                                    "name is required");
    }
    std::vector<SceneTarget> targets;
    std::string err;
    if (!parseDeviceStates(deviceStates, targets, err)) {
        return ServiceResult::error(drogon::k400BadRequest, err);
    }
    if (targets.empty()) {
        return ServiceResult::error(drogon::k400BadRequest,
                                    "device_states must not be empty");
    }

    Scene s;
    s.scene_id = http_util::newId("scn");
    s.space_id = spaceId;
    s.name = name;
    s.kind = "custom";
    s.device_states = deviceStates;
    sceneRepo_->upsert(s);
    return ServiceResult::ok(
        nlohmann::json{{"ok", true}, {"scene_id", s.scene_id}});
}

ServiceResult SceneService::update(const std::string& userId,
                                   const std::string& role,
                                   const std::string& sceneId,
                                   std::optional<std::string> name,
                                   std::optional<std::string> deviceStates) {
    const auto scene = sceneRepo_->findById(sceneId);
    if (!scene) {
        return ServiceResult::error(drogon::k404NotFound, "scene not found");
    }
    if (!userSpaces_ ||
        !authz::canAccessSpace(role, userId, scene->space_id, *userSpaces_)) {
        return ServiceResult::error(drogon::k403Forbidden,
                                    "no permission for this space");
    }

    Scene s = *scene;
    if (name.has_value()) {
        if (name->empty()) {
            return ServiceResult::error(drogon::k400BadRequest,
                                        "name must not be empty");
        }
        s.name = *name;
    }
    if (deviceStates.has_value()) {
        std::vector<SceneTarget> targets;
        std::string err;
        if (!parseDeviceStates(*deviceStates, targets, err)) {
            return ServiceResult::error(drogon::k400BadRequest, err);
        }
        if (targets.empty()) {
            return ServiceResult::error(drogon::k400BadRequest,
                                        "device_states must not be empty");
        }
        s.device_states = *deviceStates;
    }

    sceneRepo_->upsert(s);
    return ServiceResult::ok(nlohmann::json{{"ok", true}});
}

ServiceResult SceneService::remove(const std::string& userId,
                                   const std::string& role,
                                   const std::string& sceneId) {
    const auto scene = sceneRepo_->findById(sceneId);
    if (!scene) {
        return ServiceResult::error(drogon::k404NotFound, "scene not found");
    }
    if (!userSpaces_ ||
        !authz::canAccessSpace(role, userId, scene->space_id, *userSpaces_)) {
        return ServiceResult::error(drogon::k403Forbidden,
                                    "no permission for this space");
    }

    sceneRepo_->remove(sceneId);
    // 若删除的是当前激活场景，一并清空激活态。
    const auto space = spaceRepo_->findById(scene->space_id);
    if (space && space->active_scene_id == sceneId) {
        spaceRepo_->clearActiveScene(scene->space_id);
    }
    return ServiceResult::ok(nlohmann::json{{"ok", true}});
}

void SceneService::execute(const std::string& userId, const std::string& role,
                           const std::string& jwt, const std::string& sceneId,
                           Callback cb) {
    const auto scene = sceneRepo_->findById(sceneId);
    if (!scene) {
        cb(SceneResult::error(drogon::k404NotFound, "scene not found"));
        return;
    }
    if (!userSpaces_ ||
        !authz::canAccessSpace(role, userId, scene->space_id, *userSpaces_)) {
        cb(SceneResult::error(drogon::k403Forbidden,
                              "no permission for this space"));
        return;
    }

    if (scene->kind == "custom") {
        std::vector<SceneTarget> targets;
        std::string err;
        if (!parseDeviceStates(scene->device_states, targets, err)) {
            cb(SceneResult::error(drogon::k400BadRequest, err));
            return;
        }
        dispatch(userId, jwt, *scene, targets, std::move(cb));
        return;
    }

    // all_on / all_off：枚举设备 → 规则匹配 → 下发。
    const auto& defs = sceneDefinitions();
    const auto it = defs.find(scene->kind);
    if (it == defs.end()) {
        cb(SceneResult::error(drogon::k400BadRequest, "unknown scene kind"));
        return;
    }
    const std::vector<SceneRule> rules = it->second;
    const std::string spaceId = scene->space_id;

    goBackend_->get(
        UpstreamConfig::instance().devicesListPath,
        UpstreamConfig::instance().devicesSpaceIdParam + "=" + spaceId,
        GoBackendClient::bearer(jwt),
        [this, cb = std::move(cb), rules, scene = *scene, userId, jwt](
            const GoBackendResponse& r) {
            if (!r.ok) {
                cb(SceneResult::errorWithCode(
                    drogon::k503ServiceUnavailable, "GO_BACKEND_UNAVAILABLE",
                    r.errmsg.empty() ? "go-backend unreachable" : r.errmsg));
                return;
            }
            auto parsed = nlohmann::json::parse(r.body, nullptr, false);
            if (parsed.is_discarded() || !parsed.is_array()) {
                cb(SceneResult::error(
                    drogon::k502BadGateway,
                    "invalid devices response from go-backend"));
                return;
            }

            std::vector<SceneTarget> targets;
            for (const auto& dev : parsed) {
                const std::string deviceId =
                    dev.value(UpstreamConfig::instance().devIdKey, "");
                if (deviceId.empty()) {
                    continue;
                }
                for (const auto& rule : rules) {
                    if (matchesSceneRule(rule, dev)) {
                        targets.push_back(
                            {dev.value(UpstreamConfig::instance().devTypeKey, ""),
                             deviceId, rule.command});
                        break;
                    }
                }
            }
            dispatch(userId, jwt, scene, targets, std::move(cb));
        });
}

void SceneService::dispatch(const std::string& userId, const std::string& jwt,
                            const Scene& scene,
                            const std::vector<SceneTarget>& targets,
                            Callback cb) {
    if (targets.empty()) {
        SceneResult out;
        out.status = drogon::k200OK;
        out.warning = "未匹配到可控制设备";
        out.buildSummary(scene.scene_id, scene.space_id);
        writeOperationLog(userId, scene.space_id, scene.scene_id, out.results);
        cb(out);
        return;
    }

    auto acc = std::make_shared<std::vector<SceneDeviceResult>>();
    acc->reserve(targets.size());
    auto remaining = std::make_shared<int>(static_cast<int>(targets.size()));

    for (const auto& t : targets) {
        const std::string path = UpstreamConfig::fill(
            UpstreamConfig::instance().devicesControlPath,
            {{"device_type", t.type}, {"device_id", t.id}});
        const std::string body =
            nlohmann::json{{UpstreamConfig::instance().controlCommandKey,
                            t.command}}
                .dump();
        goBackend_->post(
            path, body, GoBackendClient::bearer(jwt),
            [this, cb, acc, remaining, scene, userId, t](
                const GoBackendResponse& r) {
                SceneDeviceResult dr;
                dr.device_id = t.id;
                dr.command = t.command;
                dr.status = r.ok ? static_cast<int>(r.status) : 0;
                acc->push_back(dr);

                if (--(*remaining) != 0) {
                    return;
                }

                SceneResult out;
                out.status = drogon::k200OK;
                out.results = *acc;
                const bool anyOk = std::any_of(
                    acc->begin(), acc->end(),
                    [](const SceneDeviceResult& d) {
                        return d.status >= 200 && d.status < 300;
                    });
                if (!anyOk) {
                    out.warning = "设备命令下发失败";
                } else {
                    // 至少一台下发成功才视为"已激活"。
                    spaceRepo_->setActiveScene(scene.space_id, scene.scene_id);
                }
                out.buildSummary(scene.scene_id, scene.space_id);
                writeOperationLog(userId, scene.space_id, scene.scene_id, *acc);
                cb(out);
            });
    }
}

bool SceneService::parseDeviceStates(const std::string& deviceStates,
                                     std::vector<SceneTarget>& out,
                                     std::string& err) const {
    const auto j = nlohmann::json::parse(deviceStates, nullptr, false);
    if (j.is_discarded() || !j.is_array()) {
        err = "device_states must be a JSON array";
        return false;
    }
    for (const auto& item : j) {
        if (!item.is_object() ||
            !item.contains("device_id") || !item["device_id"].is_string() ||
            !item.contains("device_type") || !item["device_type"].is_string() ||
            !item.contains("command") || !item["command"].is_string()) {
            err = "each device must have device_id/device_type/command";
            return false;
        }
        const std::string cmd = item["command"].get<std::string>();
        if (cmd != "on" && cmd != "off") {
            err = "command must be on/off";
            return false;
        }
        out.push_back({item["device_type"].get<std::string>(),
                       item["device_id"].get<std::string>(), cmd});
    }
    return true;
}

void SceneService::writeOperationLog(
    const std::string& userId, const std::string& spaceId,
    const std::string& sceneId,
    const std::vector<SceneDeviceResult>& results) {
    int success = 0;
    int failed = 0;
    nlohmann::json detail = nlohmann::json::array();
    for (const auto& d : results) {
        detail.push_back({{"device_id", d.device_id},
                          {"command", d.command},
                          {"status", d.status}});
        if (d.status >= 200 && d.status < 300) {
            ++success;
        } else {
            ++failed;
        }
    }

    OperationLogEntry entry;
    entry.op_type = "scene_execute";
    entry.user_id = userId;
    entry.space_id = spaceId;
    entry.scene_id = sceneId;
    entry.success_count = success;
    entry.failed_count = failed;
    entry.detail = detail.dump();

    const bool dbOk =
        oplog::writeOperationLog(opLogRepo_.get(), entry, opLogFile_);
    if (!dbOk && opLogFile_.empty()) {
        LOG_ERROR << "operation log write failed and no file fallback "
                     "configured (op_type="
                  << entry.op_type << ")";
    }
}
