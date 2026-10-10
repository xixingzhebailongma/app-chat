#pragma once

#include <algorithm>
#include <cctype>
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "clients/IUpstreamGateway.h"

// 一键场景的规则定义与匹配逻辑（设计文档 7.4②）。规则与匹配逻辑都在此文件，
// SceneService 只负责编排（枚举 → 匹配 → 下发 → 聚合 → 记日志）；
// 后续微调场景（增删场景、改命令、改匹配条件）只改此文件。
struct SceneRule {
    std::string device_type;                  // 必填：精确匹配 zigbee / fuhe-screen / fuhe-board
    std::string zb_role;                      // 保留字段：真实边侧无 zb_role（仅 zb_type=switch/sensor），
                                              // 生产规则留空、按 label 兜底；供未来边侧补 zb_role 时启用
    std::vector<std::string> label_include;   // 可选：label 任一命中才入选（label 兜底路径用）
    std::vector<std::string> label_exclude;   // 可选：label 任一命中即排除
    std::string command;                      // on / off
};

// 大小写归一化（label / zb_role 比较用，避免英文 light/Light 漏匹配）。
inline std::string lowerStr(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return r;
}

// 单条规则是否命中某设备。语义：
//   - device_type 必须相等（唯一必填条件）；
//   - zb_role / label_include / label_exclude 均为可选，空 = 不检查；
//   - label 比较大小写不敏感（lower 后子串匹配）。
// 纯 label 策略下灯类规则 zb_role 留空、只写 label_include，故不检查 zb_role。
inline bool matchesSceneRule(const SceneRule& rule,
                             const nlohmann::json& device) {
    const auto& cfg = UpstreamConfig::instance();
    if (!device.is_object()) {
        return false;
    }
    if (device.value(cfg.devTypeKey, "") != rule.device_type) {
        return false;
    }

    if (!rule.zb_role.empty()) {
        const std::string role =
            device.contains(cfg.devZbRoleKey) &&
                    device[cfg.devZbRoleKey].is_string()
                ? lowerStr(device[cfg.devZbRoleKey].get<std::string>())
                : "";
        if (role != lowerStr(rule.zb_role)) {
            return false;
        }
    }

    const std::string label = lowerStr(device.value(cfg.devLabelKey, ""));
    if (!rule.label_include.empty()) {
        bool any = false;
        for (const auto& kw : rule.label_include) {
            if (label.find(lowerStr(kw)) != std::string::npos) {
                any = true;
                break;
            }
        }
        if (!any) {
            return false;
        }
    }
    for (const auto& kw : rule.label_exclude) {
        if (label.find(lowerStr(kw)) != std::string::npos) {
            return false;
        }
    }
    return true;
}

// 灯类设备关键字（label 兜底匹配用）。真实边侧设备列表无 zb_role，故灯类规则
// zb_role 留空、只按 label 关键字匹配。命名规范要求灯类设备 label 必含「灯/照明」
// （中文）或 light/lamp/lighting（英文）。若使用非常规命名（如 LED 面板），部署时
// 补充此列表；若关键字可能误匹配，可用 label_exclude 精确排除。
inline const std::vector<std::string>& lightKeywords() {
    static const std::vector<std::string> kws = {
        "灯", "照明", "灯光", "吊灯", "壁灯", "筒灯", "射灯",
        "light", "lamp", "lighting",
    };
    return kws;
}

// 场景目录。门锁两个场景都不包含：门禁 off=开锁，绝不下发（7.4② 已定：场景不碰门，
// 门禁控制走手动通道 + door_devices 白名单二次确认，见 DeviceControlService）。
// fuhe-encoder/decoder 刻意不纳入 lesson_on/off：编码器是推流设备，非显示/照明，
// 开关场景不涉及（综合屏 API 文档已确认 fuhe-encoder 为真实设备类型，仍排除）。
// 若后续需要控制编码器，新增独立场景（如 encoder_on / encoder_off），不并入 lesson 场景。
inline const std::map<std::string, std::vector<SceneRule>>& sceneDefinitions() {
    static const std::map<std::string, std::vector<SceneRule>> scenes = {
        {"lesson_on",
         {SceneRule{"fuhe-screen", "", {}, {}, "on"},
          SceneRule{"fuhe-board", "", {}, {}, "on"},
          SceneRule{"zigbee", "", lightKeywords(), {}, "on"}}},
        {"lesson_off",
         {SceneRule{"fuhe-screen", "", {}, {}, "off"},
          SceneRule{"fuhe-board", "", {}, {}, "off"},
          SceneRule{"zigbee", "", lightKeywords(), {}, "off"}}},
        {"all_on",
         {SceneRule{"fuhe-screen", "", {}, {}, "on"},
          SceneRule{"fuhe-board", "", {}, {}, "on"},
          SceneRule{"zigbee", "", lightKeywords(), {}, "on"}}},
        {"all_off",
         {SceneRule{"fuhe-screen", "", {}, {}, "off"},
          SceneRule{"fuhe-board", "", {}, {}, "off"},
          SceneRule{"zigbee", "", lightKeywords(), {}, "off"}}},
    };
    return scenes;
}
