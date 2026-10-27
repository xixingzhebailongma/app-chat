#pragma once

#include <nlohmann/json.hpp>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "db/SpaceRepository.h"
#include "db/SpaceTypeRepository.h"
#include "db/UserSpacesRepository.h"

class DeviceService;
class DeviceControlService;
class SceneService;
class AlertService;
class AccessRecordService;
class SpaceBindingService;

// AI 工具注册表与处理器：把 LLM 的 tool_calls 映射到现有网关服务。
// 这是大模型唯一的数据/控制通道——没有 SQL、没有直连 go-backend 的旁路。
// 鉴权由各服务内部已实现的 authz::canAccessSpace 完成，本层不重复放行。
class AiTools {
public:
    // 一次对话中贯穿所有工具调用的身份上下文。resolvedTeacherIds /
    // resolvedSpaceIds 由 list_teachers / list_spaces 填充，作为 bind_space /
    // unbind_space 的硬约束：user_id / space_id 必须来自这两次解析，禁止猜测。
    struct Context {
        std::string userId;
        std::string role;
        std::string jwt;
        std::set<std::string> resolvedTeacherIds;
        std::set<std::string> resolvedSpaceIds;
        std::set<std::string> resolvedAlertIds;  // query_alerts 解析出的 alert_id（handle_alert 硬约束）
    };

    AiTools(std::shared_ptr<DeviceService> deviceService,
            std::shared_ptr<DeviceControlService> deviceControl,
            std::shared_ptr<SceneService> sceneService,
            std::shared_ptr<AlertService> alertService,
            std::shared_ptr<AccessRecordService> accessRecordService,
            std::shared_ptr<SpaceRepository> spaceRepo,
            std::shared_ptr<UserSpacesRepository> userSpaces,
            std::shared_ptr<SpaceTypeRepository> spaceTypes,
            std::shared_ptr<SpaceBindingService> spaceBinding,
            bool enableDeviceControl);

    // 给 LLM 的 tools 数组（OpenAI function-calling 契约）。
    std::vector<nlohmann::json> descriptors() const;

    // 执行一次工具调用；返回作为 tool 消息 content 的 JSON 值。
    // 未知工具/非法参数返回 {"error": "..."}，让模型自纠。
    nlohmann::json invoke(const std::string& name, const nlohmann::json& args,
                          Context& ctx) const;

    // 用户可访问的空间清单，供 AiService 注入 system prompt。
    // 规则与 SpaceService::list 一致：admin 全部启用空间 / teacher 仅绑定空间。
    std::vector<Space> accessibleSpaces(const std::string& userId,
                                        const std::string& role) const;

private:
    std::shared_ptr<DeviceService> deviceService_;
    std::shared_ptr<DeviceControlService> deviceControl_;
    std::shared_ptr<SceneService> sceneService_;
    std::shared_ptr<AlertService> alertService_;
    std::shared_ptr<AccessRecordService> accessRecordService_;
    std::shared_ptr<SpaceRepository> spaceRepo_;
    std::shared_ptr<UserSpacesRepository> userSpaces_;
    std::shared_ptr<SpaceTypeRepository> spaceTypes_;
    std::shared_ptr<SpaceBindingService> spaceBinding_;
    bool enableDeviceControl_ = false;  // 单设备控制开关（首期默认关，见 config.json llm.enable_device_control）
};
