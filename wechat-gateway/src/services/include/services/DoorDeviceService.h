#pragma once

#include <memory>
#include <string>

#include "db/DoorDeviceRepository.h"
#include "db/OperationLogRepository.h"
#include "db/UserSpacesRepository.h"
#include "utils/ServiceResult.h"

// 门禁白名单管理（7.5③）：list 对 admin/teacher 可读（按空间过滤），
// mark/unmark 仅 admin。mark/unmark 写 operation_logs（door_mark/door_unmark）。
class DoorDeviceService {
public:
    DoorDeviceService(std::shared_ptr<DoorDeviceRepository> repo,
                      std::shared_ptr<UserSpacesRepository> userSpaces,
                      std::shared_ptr<OperationLogRepository> opLogRepo,
                      std::string opLogFile);

    // spaceId 空：admin = 全部；teacher = 空集（不泄露）。
    ServiceResult list(const std::string& userId, const std::string& role,
                       const std::string& spaceId);

    ServiceResult mark(const std::string& userId, const std::string& role,
                       const std::string& deviceId, const std::string& spaceId,
                       const std::string& label);

    ServiceResult unmark(const std::string& userId, const std::string& role,
                         const std::string& spaceId, const std::string& deviceId);

private:
    std::shared_ptr<DoorDeviceRepository> repo_;
    std::shared_ptr<UserSpacesRepository> userSpaces_;
    std::shared_ptr<OperationLogRepository> opLogRepo_;  // 可空（内存模式）
    std::string opLogFile_;
};
