#pragma once

#include <map>
#include <string>
#include <vector>

// 渠道能力声明。NotifyService 据此做通用编排，不再比对渠道名。
struct ChannelCapabilities {
    bool requiresTarget = false;  // 需 (user_id -> 渠道外部地址) 解析；解析由
                                  // NotifyService 统一完成，结果写入 ChannelPayload::target
    bool consumesQuota = false;   // 订阅额度生命周期（预扣/回补/退订）。额度是
                                  // 「按模板」语义、本质微信订阅专属，声明为主、实现
                                  // 留在 wechat_miniapp 渠道内部，不抽象进通用层
    bool isFallback = false;      // 可作兜底渠道（NotifyService 据此定位 fallback）
};

// 交给渠道实现的负载。只含跨渠道通用的业务字段；渠道专属字段（小程序模板/
// data、到校 student_*）分别留在渠道内部与独立 OaPayload，不进本结构。
struct ChannelPayload {
    std::string event_id;
    std::string event_type;
    std::string content;
    std::string severity;
    std::string space_id;
    std::string device_id;
    std::string device_label;
    std::vector<std::string> roles;
    bool space_teachers = false;

    // 已解析的接收者 user_id（设计文档 9.4 第 2 步）。
    std::vector<std::string> recipients;

    // requiresTarget 时由 NotifyService 填的渠道外部地址（openid / phone / userid）。
    std::string target;
};

struct ChannelResult {
    std::string channel;
    bool success = false;
    std::string message;
    int errcode = 0;      // 平台返回的 errcode（0 = 无错误）
    std::string errmsg;   // 平台 errmsg 或本地错误描述
    bool unsubscribed = false;  // 用户已永久退订此渠道（如 43101），供日志/观测
};

class INotifyChannel {
public:
    virtual ~INotifyChannel() = default;

    virtual std::string name() const = 0;

    // 渠道是否启用（设计文档 9.1）。禁用的渠道会被注册但在分发时跳过。
    virtual bool enabled() const { return true; }

    virtual ChannelCapabilities capabilities() const { return {}; }

    virtual ChannelResult send(const ChannelPayload& payload) = 0;
};
