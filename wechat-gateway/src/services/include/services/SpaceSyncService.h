#pragma once

#include <memory>
#include <vector>

#include "db/DoorDeviceRepository.h"
#include "db/SpaceRepository.h"
#include "db/UserSpacesRepository.h"
#include "utils/ServiceResult.h"

// 空间生命周期同步（设计文档 7.7 收尾项 e）：接收边侧回灌的全量空间列表，
// 对账网关本地 spaces 缓存，并级联清理 user_spaces / door_devices 的孤儿行。
// 权威空间数据仍在边侧；本服务只做「全量替换 + 幂等收敛」。
class SpaceSyncService {
public:
    SpaceSyncService(std::shared_ptr<SpaceRepository> spaces,
                     std::shared_ptr<UserSpacesRepository> userSpaces,
                     std::shared_ptr<DoorDeviceRepository> doorDevices);

    // 全量对账：upsert 给定空间，删除不在给定列表里的空间，并级联清理
    // 被删空间的 user_spaces / door_devices。幂等——重复回灌同一份全量列表
    // 收敛到同一状态（非单条 DB 事务，中途失败由下次全量回灌自愈）。
    // 注意：调用方必须回灌「全量列表」，否则缺失项会被当作删除误清。
    ServiceResult sync(const std::vector<Space>& spaces);

private:
    std::shared_ptr<SpaceRepository> spaces_;
    std::shared_ptr<UserSpacesRepository> userSpaces_;
    std::shared_ptr<DoorDeviceRepository> doorDevices_;
};
