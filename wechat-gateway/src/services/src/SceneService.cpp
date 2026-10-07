#include "services/SceneService.h"

#include <algorithm>
#include <utility>

#include <drogon/drogon.h>

#include "db/OperationLogWriter.h"
#include "services/SceneDefinitions.h"
#include "utils/Authz.h"

SceneService::SceneService(std::shared_ptr<GoBackendClient> goBackend,
                           std::shared_ptr<UserSpacesRepository> spaces,
                           std::shared_ptr<OperationLogRepository> opLogRepo,
                           std::string opLogFile)
    : goBackend_(std::move(goBackend)),
      spaces_(std::move(spaces)),
      opLogRepo_(std::move(opLogRepo)),
      opLogFile_(std::move(opLogFile)) {}

void SceneService::execute(const std::string& userId, const std::string& role,
                           const std::string& jwt, const std::string& spaceId,
                           const std::string& sceneId, Callback cb) {
    const auto& scenes = sceneDefinitions();
    const auto it = scenes.find(sceneId);
    if (it == scenes.end()) {
        cb(SceneResult::error(
            drogon::k400BadRequest,
            "unknown scene; supported: lesson_on / lesson_off"));
        return;
    }
    const std::vector<SceneRule> rules = it->second;

    // 权限检查（与 DeviceControlService::control 一致）：
    // 管理员始终允许；教师必须已绑定该空间；其他人拒绝。
    if (!spaces_ || !authz::canAccessSpace(role, userId, spaceId, *spaces_)) {
        cb(SceneResult::error(drogon::k403Forbidden,
                              "no permission for this space"));
        return;
    }

    // 枚举该空间设备（同 DeviceService::listDevices 的 go-backend 调用）。
    goBackend_->get(
        UpstreamConfig::instance().devicesListPath,
        UpstreamConfig::instance().devicesSpaceIdParam + "=" + spaceId,
        GoBackendClient::bearer(jwt),
        [this, cb = std::move(cb), rules, spaceId, sceneId, userId, jwt](
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

            // 过滤出要下发的 (device_type, device_id, command) 列表。
            struct Target {
                std::string type;
                std::string id;
                std::string command;
            };
            std::vector<Target> targets;
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
                        break;  // 一台设备只按首条命中规则下发一次
                    }
                }
            }

            if (targets.empty()) {
                SceneResult out;
                out.status = drogon::k200OK;
                out.warning = "未匹配到可控制设备";
                out.buildSummary(sceneId, spaceId);
                writeOperationLog(userId, spaceId, sceneId, out.results);
                cb(out);
                return;
            }

            auto acc = std::make_shared<std::vector<SceneDeviceResult>>();
            acc->reserve(targets.size());
            auto remaining =
                std::make_shared<int>(static_cast<int>(targets.size()));

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
                    [this, cb, acc, remaining, spaceId, sceneId, userId, jwt, t](
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
                        }
                        out.buildSummary(sceneId, spaceId);
                        writeOperationLog(userId, spaceId, sceneId, *acc);
                        cb(out);
                    });
            }
        });
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
    if (!dbOk) {
        if (opLogFile_.empty()) {
            LOG_ERROR << "operation log write failed and no file fallback "
                         "configured (op_type="
                      << entry.op_type << ")";
        } else {
            LOG_ERROR << "operation log DB write failed; file fallback "
                         "attempted at "
                      << opLogFile_;
        }
    }
}
