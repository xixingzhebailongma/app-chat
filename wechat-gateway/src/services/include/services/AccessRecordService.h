#pragma once

#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include "db/AccessRecordRepository.h"
#include "db/UserSpacesRepository.h"
#include "utils/ServiceResult.h"

// 进出记录（设计文档 7.4 / 7.5）：list 执行空间边界（管理员：全部；教师：仅其
// 绑定教室）并按日期/分页查询；ingest 供边侧经内部端点写入（event_id 幂等）。
class AccessRecordService {
public:
    AccessRecordService(std::shared_ptr<AccessRecordRepository> records,
                        std::shared_ptr<UserSpacesRepository> userSpaces);

    ServiceResult list(const std::string& userId, const std::string& role,
                       const std::string& spaceId, const std::string& date,
                       int page, int pageSize, const std::string& authType = "");
    ServiceResult ingest(const nlohmann::json& body);

private:
    std::shared_ptr<AccessRecordRepository> records_;
    std::shared_ptr<UserSpacesRepository> userSpaces_;
};
