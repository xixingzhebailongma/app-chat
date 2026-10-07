#pragma once

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "db/SubscriptionRepository.h"
#include "utils/ServiceResult.h"

// 小程序订阅授权（设计文档 八 订阅授权）——模板级额度模型。
// knownTemplateIds：配置里的模板 ID 集合（用于校验与回显）。
// longTermTemplateIds：其中 long_term 的模板 ID（grant 时置 -1）。
class SubscriptionService {
public:
    SubscriptionService(std::shared_ptr<SubscriptionRepository> subs,
                        std::set<std::string> knownTemplateIds,
                        std::set<std::string> longTermTemplateIds);

    // 上报授权：只上报 accept 的模板 ID，逐个 grant。
    ServiceResult applyGrants(const std::string& userId,
                              const std::vector<std::string>& acceptedTemplates);

    // 关闭订阅：清空该用户全部模板额度。
    ServiceResult unsubscribeAll(const std::string& userId);

    // 回显：返回 { templates: { tid: { active, quota } } }。
    ServiceResult state(const std::string& userId);

    // 模板 ID 权威来源（GET /api/miniapp/notify/templates）：返回
    // { template_ids: [...] }（去重、排序），供小程序端 requestSubscribeMessage
    // 一次性拉取真实模板 ID，替代构建期 env 硬编码。
    ServiceResult templateIds();

private:
    std::shared_ptr<SubscriptionRepository> subs_;
    std::set<std::string> knownTemplateIds_;
    std::set<std::string> longTermTemplateIds_;
};
