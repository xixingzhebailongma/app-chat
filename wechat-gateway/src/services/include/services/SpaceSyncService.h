#pragma once

#include <memory>
#include <vector>

#include "db/DoorDeviceRepository.h"
#include "db/SpaceRepository.h"
#include "db/UserSpacesRepository.h"
#include "utils/ServiceResult.h"

// 空间生命周期同步（边侧回灌）：接收边侧回灌的全量空间列表，补录网关没有的
// 教室，对账 source=edge 的教室，并级联清理 user_spaces / door_devices 孤儿行。
// 网关是空间权威来源：source=admin 的教室不覆盖、不删除；source=edge 的照旧。
class SpaceSyncService {
public:
    SpaceSyncService(std::shared_ptr<SpaceRepository> spaces,
                     std::shared_ptr<UserSpacesRepository> userSpaces,
                     std::shared_ptr<DoorDeviceRepository> doorDevices);

    // 全量对账：补录给定空间（source=admin 不覆盖），删除 source=edge 且不在
    // 给定列表里的空间，并级联清理被删空间的 user_spaces / door_devices。
    // 幂等——重复回灌同一份全量列表收敛到同一状态。
    // 注意：调用方必须回灌「全量列表」，否则缺失项会被当作删除误清。
    ServiceResult sync(const std::vector<Space>& spaces);

private:
    std::shared_ptr<SpaceRepository> spaces_;
    std::shared_ptr<UserSpacesRepository> userSpaces_;
    std::shared_ptr<DoorDeviceRepository> doorDevices_;
};
