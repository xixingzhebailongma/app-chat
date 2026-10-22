#pragma once

#include <nlohmann/json.hpp>

#include <memory>
#include <string>
#include <vector>

#include "db/SpaceRepository.h"
#include "db/UserSpacesRepository.h"

class DeviceService;
class DeviceControlService;
class SceneService;
class AlertService;
class AccessRecordService;

// AI 工具注册表与处理器：把 LLM 的 tool_calls 映射到现有网关服务。
// 这是大模型唯一的数据/控制通道——没有 SQL、没有直连 go-backend 的旁路。
// 鉴权由各服务内部已实现的 authz::canAccessSpace 完成，本层不重复放行。
class AiTools {
public:
    // 一次对话中贯穿所有工具调用的身份上下文。
    struct Context {
        std::string userId;
        std::string role;
        std::string jwt;
    };

    AiTools(std::shared_ptr<DeviceService> deviceService,
            std::shared_ptr<DeviceControlService> deviceControl,
            std::shared_ptr<SceneService> sceneService,
            std::shared_ptr<AlertService> alertService,
            std::shared_ptr<AccessRecordService> accessRecordService,
            std::shared_ptr<SpaceRepository> spaceRepo,
            std::shared_ptr<UserSpacesRepository> userSpaces,
            bool enableDeviceControl);

    // 给 LLM 的 tools 数组（OpenAI function-calling 契约）。
    std::vector<nlohmann::json> descriptors() const;

    // 执行一次工具调用；返回作为 tool 消息 content 的 JSON 值。
    // 未知工具/非法参数返回 {"error": "..."}，让模型自纠。
    nlohmann::json invoke(const std::string& name, const nlohmann::json& args,
                          const Context& ctx) const;

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
    bool enableDeviceControl_ = false;  // 单设备控制开关（首期默认关，见 config.json llm.enable_device_control）
};
