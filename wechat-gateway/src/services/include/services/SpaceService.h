#pragma once

#include <memory>
#include <string>

#include "db/SpaceRepository.h"
#include "db/UserSpacesRepository.h"
#include "utils/ServiceResult.h"

// GET /api/miniapp/spaces（设计文档 八 纯网关业务接口）：管理员可查看每个
// 空间；教师仅能查看其通过 user_spaces 绑定的空间。
class SpaceService {
public:
    SpaceService(std::shared_ptr<SpaceRepository> spaces,
                 std::shared_ptr<UserSpacesRepository> userSpaces);

    ServiceResult list(const std::string& userId, const std::string& role);

private:
    std::shared_ptr<SpaceRepository> spaces_;
    std::shared_ptr<UserSpacesRepository> userSpaces_;
};
