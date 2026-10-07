#include "services/SpaceSyncService.h"

#include <nlohmann/json.hpp>

#include <string>
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
    // 给定列表去重（空 space_id 忽略，防脏数据），构造本轮应存在的集合。
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

    // 现存 space_id 集合，用于计算「应删除」的差集。
    std::unordered_set<std::string> current;
    for (const auto& s : spaces_->listAll()) {
        current.insert(s.space_id);
    }

    // 1) upsert 给定项。
    for (const auto& s : toUpsert) {
        spaces_->upsert(s);
    }

    // 2) 删除缺失项，并级联清理孤儿绑定 / 门禁标记。
    int removed = 0;
    for (const auto& id : current) {
        if (provided.count(id) == 0) {
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
        {"ok", true}, {"upserted", toUpsert.size()}, {"removed", removed}});
}
