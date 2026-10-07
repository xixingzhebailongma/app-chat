#pragma once

#include <memory>

#include "channels/WechatOaChannel.h"
#include "db/OaNotifyLogRepository.h"
#include "db/ParentBindingRepository.h"
#include "dto/ArrivalNotifyDto.h"

class OaNotifyService {
public:
    OaNotifyService(std::shared_ptr<ParentBindingRepository> bindingRepo,
                    std::shared_ptr<WechatOaChannel> oaChannel,
                    std::shared_ptr<OaNotifyLogRepository> logRepo,
                    bool notifyOncePerDay = true, bool notifyOnLeave = false);

    ArrivalNotifyResponse notify(const ArrivalNotifyRequest& req);

private:
    std::shared_ptr<ParentBindingRepository> bindingRepo_;
    std::shared_ptr<WechatOaChannel> oaChannel_;
    std::shared_ptr<OaNotifyLogRepository> logRepo_;
    bool notifyOncePerDay_;
    bool notifyOnLeave_;
};
