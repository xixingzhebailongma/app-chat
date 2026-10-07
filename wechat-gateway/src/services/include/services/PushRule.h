#pragma once

#include <functional>
#include <set>
#include <string>
#include <vector>

#include "dto/NotifySendDto.h"

// 推送分级规则（设计文档 九 9.2）。将 event_type 映射到其目标受众、
// 冷却时间与主渠道。`condition` 是对 7.6 发送规则的防御性复查：
// 生产者（go-backend）才是这些规则的权威，因此规则仅当请求携带其测试的
// 属性且该属性未通过检查时才抑制该事件。
struct PushRule {
    std::string event_type;
    std::set<std::string> target_roles;  // 例如 {"admin"} / {"admin","teacher"}
    bool to_space_teacher = false;       // 同时推送给该空间的教师
    int cooldown_seconds = 1800;
    std::vector<std::string> channels;   // 有序的主渠道
    std::function<bool(const NotifySendRequest&)> condition;
};

// 四种可推送的事件类型（设计文档 9.2）。"face_login_success" 被有意排除 ——
// 绝不能主动推送它，因此未知或不可推送的 event_type 匹配不到任何规则并会被跳过。
inline const std::vector<PushRule>& defaultPushRules() {
    static const std::vector<PushRule> rules = {
        {"device_offline",
         {"admin"},
         true,
         1800,
         {"wechat_miniapp"},
         [](const NotifySendRequest& r) {
             return r.transition.empty() || r.transition == "online->offline";
         }},
        {"sensor_threshold",
         {"admin"},
         true,
         1800,
         {"wechat_miniapp"},
         [](const NotifySendRequest& r) {
             return r.duration_sec < 0 || r.duration_sec >= 300;
         }},
        {"face_login_failed",
         {"admin"},
         false,
         1800,
         {"wechat_miniapp"},
         [](const NotifySendRequest& r) {
             return r.consecutive < 0 || r.consecutive >= 3;
         }},
        {"peripheral_offline",
         {"admin"},
         false,
         1800,
         {"wechat_miniapp"},
         [](const NotifySendRequest& r) {
             return r.offline_minutes < 0 || r.offline_minutes > 30;
         }},
    };
    return rules;
}
