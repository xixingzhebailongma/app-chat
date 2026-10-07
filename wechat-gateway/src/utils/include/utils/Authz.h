#pragma once

#include <string>

#include "db/UserSpacesRepository.h"

// 空间范围鉴权规则的唯一事实来源（设计文档
// 7.3 / 八）：admin 可访问所有空间，teacher 只能访问通过 user_spaces
// 绑定的空间，其他人无访问权限。设备读取、
// 设备控制和告警列表路径均复用此规则，避免规则在各处漂移。
namespace authz {

inline bool canAccessSpace(const std::string& role,
                           const std::string& userId,
                           const std::string& spaceId,
                           const UserSpacesRepository& spaces) {
    if (role == "admin") {
        return true;
    }
    if (role == "teacher") {
        return spaces.contains(userId, spaceId);
    }
    return false;
}

}  // namespace authz
