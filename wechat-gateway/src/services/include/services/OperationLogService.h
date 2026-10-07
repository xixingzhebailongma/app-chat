#pragma once

#include <memory>
#include <string>

#include "db/OperationLogRepository.h"
#include "utils/ServiceResult.h"

// GET /api/miniapp/operation-logs（设计文档 7.5 运维日志）。list() 仅
// 管理员可读；内存模式（repo 为空）返回空列表。
class OperationLogService {
public:
    explicit OperationLogService(std::shared_ptr<OperationLogRepository> repo);

    ServiceResult list(const std::string& userId, const std::string& role,
                       const OperationLogFilter& filter, int page, int pageSize);

private:
    std::shared_ptr<OperationLogRepository> repo_;  // 可空（内存模式）
};
