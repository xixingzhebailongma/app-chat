#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "db/SpaceTypeRepository.h"
#include "utils/ServiceResult.h"

// 空间类型管理（管理员自定义空间类型，前端动态拉取）。
// list 登录可读（返回全部含停用）；create/update/disable/reorder 仅 admin。
class SpaceTypeService {
public:
    explicit SpaceTypeService(std::shared_ptr<SpaceTypeRepository> repo);

    // GET /api/miniapp/space-types
    ServiceResult list();

    // POST /api/admin/space-types；sortOrder 无值 = 排最后（max+1）。
    ServiceResult create(const std::string& role, const std::string& code,
                         const std::string& name, const std::string& icon,
                         std::optional<int> sortOrder);

    // PUT /api/admin/space-types/{code}：只改传入的字段（合并进现有行）。
    ServiceResult update(const std::string& role, const std::string& code,
                         std::optional<std::string> name,
                         std::optional<std::string> icon,
                         std::optional<bool> enabled);

    // DELETE /api/admin/space-types/{code}：软删除（enabled=false）。
    ServiceResult disable(const std::string& role, const std::string& code);

    // PATCH /api/admin/space-types/order：按数组下标重写 sort_order。
    ServiceResult reorder(const std::string& role,
                          const std::vector<std::string>& codes);

private:
    std::shared_ptr<SpaceTypeRepository> repo_;
};
