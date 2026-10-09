#pragma once

#include <memory>
#include <optional>
#include <string>

#include "db/SpaceRepository.h"
#include "db/SpaceTypeRepository.h"
#include "db/UserSpacesRepository.h"
#include "utils/ServiceResult.h"

// 空间：miniapp 读（仅启用）+ admin 增删改（网关为空间权威来源）。
// list 登录可读；create/update/disable/adminList 仅 admin。
class SpaceService {
public:
    SpaceService(std::shared_ptr<SpaceRepository> spaces,
                 std::shared_ptr<UserSpacesRepository> userSpaces,
                 std::shared_ptr<SpaceTypeRepository> spaceTypes);

    // GET /api/miniapp/spaces：仅返回启用空间（admin 全量 / teacher 绑定）。
    ServiceResult list(const std::string& userId, const std::string& role);

    // GET /api/admin/spaces：全部（含停用 + source）。
    ServiceResult adminList(const std::string& role);

    // POST /api/admin/spaces：name/type 必填，type 必须在 space_types；
    // space_id 缺省自动生成。
    ServiceResult create(const std::string& role, const std::string& name,
                         const std::string& type, const std::string& spaceId);

    // PUT /api/admin/spaces/{space_id}：改 name/type（可选字段合并）。
    ServiceResult update(const std::string& role, const std::string& spaceId,
                         std::optional<std::string> name,
                         std::optional<std::string> type);

    // DELETE /api/admin/spaces/{space_id}：软删除（enabled=false）。
    ServiceResult disable(const std::string& role, const std::string& spaceId);

private:
    std::shared_ptr<SpaceRepository> spaces_;
    std::shared_ptr<UserSpacesRepository> userSpaces_;
    std::shared_ptr<SpaceTypeRepository> spaceTypes_;
};
