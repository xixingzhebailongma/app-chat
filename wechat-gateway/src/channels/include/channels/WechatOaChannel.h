#pragma once

#include <memory>
#include <string>
#include <utility>

#include "channels/INotifyChannel.h"  // 复用 ChannelResult（通用结果值类型）
#include "clients/WechatTokenManager.h"

// 家长到校通知的负载（独立于通用 ChannelPayload）。到校推送走 OaNotifyService
// 独立路径，不复用 NotifyService 通用分发，故字段不混入 ChannelPayload。
struct OaPayload {
    std::string student_no;
    std::string student_name;
    std::string space_name;
    std::string arrival_time;
    std::string oa_openid;    // 家长公众号 openid（touser）
    std::string template_id;  // 到校模板 ID（发送时由渠道填入）
};

// 公众号模板消息渠道（到校通知专用）。不再是 INotifyChannel：不经 ChannelFactory、
// 不参与 notify.routing，由 main 用 wechat.oa 凭证直接构造注入 OaNotifyService。
// 复用 ChannelResult 作返回值（ChannelResult 是通用结果值类型，不依赖 INotifyChannel）。
class WechatOaChannel {
public:
    explicit WechatOaChannel(
        std::shared_ptr<WechatTokenManager> tokens = nullptr,
        std::string apiBaseUrl = "https://api.weixin.qq.com",
        std::string arrivalTemplateId = "", bool enabled = true,
        bool realMode = false);

    virtual ~WechatOaChannel() = default;

    std::string name() const { return "wechat_oa"; }
    bool enabled() const { return enabled_; }

    // 是否已配置真实到校模板 ID（非空且非 tmpl_* 占位符）。real 模式下
    // 未配置属配置错误（OaNotifyService 据此返回 503），不再静默 mock。
    bool isConfigured() const {
        return !arrivalTemplateId_.empty() &&
               arrivalTemplateId_.rfind("tmpl_", 0) != 0;
    }

    virtual ChannelResult send(const OaPayload& payload);

private:
    bool enabled_;
    bool realMode_;
    std::shared_ptr<WechatTokenManager> tokens_;
    std::string apiBaseUrl_;
    std::string arrivalTemplateId_;
};
