#pragma once

#include <memory>
#include <string>

#include "db/AlertRepository.h"
#include "db/OperationLogRepository.h"
#include "db/UserSpacesRepository.h"
#include "utils/ServiceResult.h"

// GET /api/miniapp/alerts 与 POST /api/miniapp/alerts/{id}/handle（设计文档
// 八 纯网关业务接口）。list() 执行空间边界（管理员：全部；教师：仅其
// user_spaces，且隐藏 operator_id/remark）；handle() 要求管理员角色。
class AlertService {
public:
    AlertService(std::shared_ptr<AlertRepository> alerts,
                 std::shared_ptr<UserSpacesRepository> userSpaces,
                 std::shared_ptr<OperationLogRepository> opLogRepo,
                 std::string opLogFile);

    // spaceId：单教室筛选（空 = 不筛选）。filter：类型/状态/时间范围。
    // page 从 1 起；pageSize 默认 50、上限 200（服务层护栏）。
    ServiceResult list(const std::string& userId, const std::string& role,
                       const std::string& spaceId, const AlertFilter& filter,
                       int page, int pageSize);
    ServiceResult handle(const std::string& alertId, const std::string& userId,
                         const std::string& role, const std::string& remark,
                         const std::string& status);

    // GET /api/miniapp/alerts/{id}/timeline（7.5④ 处理过程 + 操作人）。
    // 仅 admin；返回告警摘要 + 按时间升序的 handle 事件列表（不分页）。
    ServiceResult timeline(const std::string& alertId, const std::string& role);

    // GET /api/miniapp/alerts/{id}（单条详情，教师订阅消息深链用）。
    // admin 返回完整字段；teacher 仅本空间（越权与不存在统一 404，不暴露存在性），
    // 且 operator_id/operator_name/remark 置空（与 list 脱敏一致）；其他角色 403。
    ServiceResult detail(const std::string& alertId, const std::string& userId,
                         const std::string& role);

    // GET /api/miniapp/alerts/stats（7.5④ 统计）。admin 全量 / teacher 绑定
    // 教室；返回 by_status / by_event_type（by_space 仅 admin）。
    ServiceResult stats(const std::string& userId, const std::string& role);

private:
    std::shared_ptr<AlertRepository> alerts_;
    std::shared_ptr<UserSpacesRepository> userSpaces_;
    std::shared_ptr<OperationLogRepository> opLogRepo_;  // 可空（内存模式）
    std::string opLogFile_;  // 空串 = 禁用文件兜底
};
