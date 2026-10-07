#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

#include <functional>
#include <memory>

#include "db/UserRoleRepository.h"

// POST /internal/user-roles/sync（InternalTokenFilter 鉴权）。
// go-backend 在登录/角色变更时调用，全量替换某用户的角色集合，
// 把权威角色数据同步进网关的 user_roles 只读模型（设计文档 14）。
class UserRoleController {
public:
    explicit UserRoleController(std::shared_ptr<UserRoleRepository> repo);

    void sync(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    std::shared_ptr<UserRoleRepository> repo_;
};
