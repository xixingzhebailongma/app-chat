#include "services/SpaceSyncService.h"

#include <nlohmann/json.hpp>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

SpaceSyncService::SpaceSyncService(
    std::shared_ptr<SpaceRepository> spaces,
    std::shared_ptr<UserSpacesRepository> userSpaces,
    std::shared_ptr<DoorDeviceRepository> doorDevices)
    : spaces_(std::move(spaces)),
      userSpaces_(std::move(userSpaces)),
      doorDevices_(std::move(doorDevices)) {}

ServiceResult SpaceSyncService::sync(const std::vector<Space>& spaces) {
    // 给定列表去重（空 space_id 忽略，防脏数据）。
    std::unordered_set<std::string> provided;
    std::vector<Space> toUpsert;
    toUpsert.reserve(spaces.size());
    for (const auto& s : spaces) {
        if (s.space_id.empty()) {
            continue;
        }
        if (provided.insert(s.space_id).second) {
            toUpsert.push_back(s);
        }
    }

    // 现存 space_id -> Space（用于区分 source + 保留 enabled）。
    std::unordered_map<std::string, Space> current;
    for (const auto& s : spaces_->listAll()) {
        current[s.space_id] = s;
    }

    // 1) 补录：source=admin 的教室不覆盖（网关权威）；其余照旧 upsert。
    //    对已存在的 edge 教室保留其 enabled，避免管理员软删除后被同步重新启用。
    int upserted = 0;
    for (auto& s : toUpsert) {
        const auto it = current.find(s.space_id);
        if (it != current.end() && it->second.source == "admin") {
            continue;
        }
        s.source = "edge";
        s.enabled = (it != current.end()) ? it->second.enabled : true;
        spaces_->upsert(s);
        ++upserted;
    }

    // 2) 删除：仅删 source=edge 且不在给定列表里的；source=admin 永不删。
    int removed = 0;
    for (const auto& [id, sp] : current) {
        if (sp.source == "edge" && provided.count(id) == 0) {
            spaces_->remove(id);
            if (userSpaces_) {
                userSpaces_->removeBySpace(id);
            }
            if (doorDevices_) {
                doorDevices_->removeBySpace(id);
            }
            ++removed;
        }
    }

    return ServiceResult::ok(nlohmann::json{
        {"ok", true}, {"upserted", upserted}, {"removed", removed}});
}
