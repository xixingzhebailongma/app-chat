#pragma once

#include <memory>
#include <vector>

#include "db/StudentParentRepository.h"
#include "utils/ServiceResult.h"

// 花名册全量对账（设计文档 7.8 §2）：接收学校/运营回灌的全量花名册，
// 对账网关本地 student_parents 缓存，幂等收敛到同一状态。权威数据仍在
// 校方；本服务只做「全量替换 + 幂等收敛」。
class StudentParentSyncService {
public:
    explicit StudentParentSyncService(
        std::shared_ptr<StudentParentRepository> parents);

    // 全量对账：upsert 给定行，删除不在给定列表里的行。幂等——重复回灌
    // 同一份全量列表收敛到同一状态。注意：调用方必须回灌「全量列表」，
    // 否则缺失项会被当作删除误清。
    ServiceResult sync(const std::vector<StudentParent>& rows);

private:
    std::shared_ptr<StudentParentRepository> parents_;
};
