#include <drogon/drogon.h>

#include <chrono>
#include <cstdlib>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>

#include <nlohmann/json.hpp>

#include "utils/ErrorCodeMapper.h"
#include "utils/ErrorEnvelope.h"

#include "channels/ChannelFactory.h"
#include "channels/WechatMiniAppChannel.h"
#include "channels/WechatOaChannel.h"
#include "clients/GoBackendClient.h"
#include "clients/SmsClient.h"
#include "clients/SmsProviderFactory.h"
#include "clients/WechatClient.h"
#include "clients/WechatTokenManager.h"
#include "clients/FallbackNotifyTargetResolver.h"
#include "controllers/AccessRecordController.h"
#include "controllers/AlertController.h"
#include "controllers/DeviceController.h"
#include "controllers/DoorDeviceController.h"
#include "controllers/MiniAppController.h"
#include "controllers/NotifyController.h"
#include "controllers/OaController.h"
#include "controllers/OperationLogController.h"
#include "controllers/SpaceBindingController.h"
#include "controllers/NotifyBindingController.h"
#include "controllers/SpaceController.h"
#include "controllers/SceneController.h"
#include "controllers/SpaceTypeController.h"
#include "controllers/SpaceSyncController.h"
#include "controllers/StudentParentSyncController.h"
#include "controllers/SubscribeNotifyController.h"
#include "controllers/SubscriptionController.h"
#include "controllers/UserRoleController.h"
#include "controllers/WsProxyController.h"
#include "db/AccessRecordRepository.h"
#include "db/AlertRepository.h"
#include "db/DoorDeviceRepository.h"
#include "db/NotifyLogRepository.h"
#include "db/NotifyTargetRepository.h"
#include "db/NotifyTargetResolver.h"
#include "db/OaNotifyLogRepository.h"
#include "db/OperationLogRepository.h"
#include "db/ParentBindingRepository.h"
#include "db/PendingEventRepository.h"
#include "db/PgAccessRecordRepository.h"
#include "db/PgAlertRepository.h"
#include "db/PgDoorDeviceRepository.h"
#include "db/PgNotifyLogRepository.h"
#include "db/PgNotifyTargetRepository.h"
#include "db/PgOaNotifyLogRepository.h"
#include "db/PgOperationLogRepository.h"
#include "db/PgParentBindingRepository.h"
#include "db/PgPendingEventRepository.h"
#include "db/PgPool.h"
#include "db/PgSpaceRepository.h"
#include "db/PgSpaceTypeRepository.h"
#include "db/PgStudentParentRepository.h"
#include "db/PgSubscriptionRepository.h"
#include "db/PgNotifyTargetResolver.h"
#include "db/PgUserRoleRepository.h"
#include "db/PgUserSpacesRepository.h"
#include "db/PgWechatBindingRepository.h"
#include "db/RedisClient.h"
#include "db/SpaceRepository.h"
#include "db/SceneRepository.h"
#include "db/PgSceneRepository.h"
#include "db/SpaceTypeRepository.h"
#include "db/StudentParentRepository.h"
#include "db/SubscriptionRepository.h"
#include "db/UserRoleRepository.h"
#include "db/UserSpacesRepository.h"
#include "db/WechatBindingRepository.h"
#include "filters/InternalTokenFilter.h"
#include "filters/JwtFilter.h"
#include "services/AccessRecordService.h"
#include "services/AlertService.h"
#include "services/AuthService.h"
#include "services/DeviceControlService.h"
#include "services/DeviceService.h"
#include "services/DoorDeviceService.h"
#include "services/NotifyService.h"
#include "services/OaBindService.h"
#include "services/OaNotifyService.h"
#include "services/OperationLogService.h"
#include "services/PushRule.h"
#include "services/RetryWorker.h"
#include "services/SceneService.h"
#include "services/SpaceBindingService.h"
#include "services/NotifyBindingService.h"
#include "services/SpaceService.h"
#include "services/SpaceSyncService.h"
#include "services/SpaceTypeService.h"
#include "services/StudentParentSyncService.h"
#include "services/SubscriptionService.h"
#include "dto/NotifySendDto.h"
#include "utils/JwtUtil.h"

using namespace drogon;

namespace {

// JsonCpp (drogon 配置) -> nlohmann::json（渠道/客户端内部统一用 nlohmann）。
nlohmann::json toNlohmann(const Json::Value& v) {
    switch (v.type()) {
        case Json::objectValue: {
            nlohmann::json o = nlohmann::json::object();
            for (const auto& k : v.getMemberNames()) {
                o[k] = toNlohmann(v[k]);
            }
            return o;
        }
        case Json::arrayValue: {
            nlohmann::json a = nlohmann::json::array();
            for (const auto& e : v) {
                a.push_back(toNlohmann(e));
            }
            return a;
        }
        case Json::stringValue:
            return v.asString();
        case Json::booleanValue:
            return v.asBool();
        case Json::intValue:
            return v.asInt64();
        case Json::uintValue:
            return v.asUInt64();
        case Json::realValue:
            return v.asDouble();
        case Json::nullValue:
        default:
            return nullptr;
    }
}

}  // namespace

int main() {
    app().loadConfigFile("config/config.json");

    const Json::Value& cfg = app().getCustomConfig();
    const Json::Value cc = cfg.isMember("custom_config") ? cfg["custom_config"]
                                                         : cfg;

    // 配置驱动适配壳：错误码表 / 错误信封 / 上游路径字段（7.9 改造）。
    // 文件缺失用内置默认；非法 JSON 抛异常 -> LOG_FATAL 启动失败（不静默走错格式）。
    try {
        ErrorCodeMapper::instance().loadFromFile("config/error_codes.json");
        ErrorEnvelope::instance().loadFromFile("config/error_envelope.json");
        UpstreamConfig::instance().loadFromFile("config/upstream.json");
    } catch (const std::exception& e) {
        LOG_FATAL << "failed to load adapter config: " << e.what();
        return 1;
    }

    const uint16_t port =
        static_cast<uint16_t>(cc.get("listen_port", 8080).asUInt());
    std::string internalToken =
        cc.get("internal_token", "dev-internal-token").asString();
    if (const char* env = std::getenv("INTERNAL_TOKEN"); env && *env) {
        internalToken = env;
    }

    // JWT（设计文档 六）：与 go-backend 共享的密钥。
    std::string jwtSecret = cc["jwt"].get("secret", "dev-shared-jwt-secret")
                                .asString();
    if (const char* env = std::getenv("JWT_SECRET"); env && *env) {
        jwtSecret = env;
    }
    long accessTtl = JwtUtil::DEFAULT_ACCESS_TTL;
    long openidTtl = JwtUtil::DEFAULT_OPENID_TTL;
    if (cc["jwt"].isMember("access_ttl_seconds")) {
        accessTtl = cc["jwt"]["access_ttl_seconds"].asInt();
    }
    if (cc["jwt"].isMember("openid_ttl_seconds")) {
        openidTtl = cc["jwt"]["openid_ttl_seconds"].asInt();
    }

    // 微信小程序凭证 + go-backend 基础 URL。
    std::string wechatAppid =
        cc["wechat"]["miniapp"].get("appid", "dev-miniapp-appid").asString();
    std::string wechatSecret =
        cc["wechat"]["miniapp"].get("secret", "dev-miniapp-secret").asString();
    if (const char* env = std::getenv("WECHAT_APPID"); env && *env) {
        wechatAppid = env;
    }
    if (const char* env = std::getenv("WECHAT_SECRET"); env && *env) {
        wechatSecret = env;
    }
    // 微信 OA（公众号）凭证，用于 access_token 缓存（设计文档 十）。
    // 密钥只通过 K8s Secret 的环境变量注入，绝不来自配置。
    std::string wechatOaAppid =
        cc["wechat"]["oa"].get("appid", "dev-oa-appid").asString();
    std::string wechatOaSecret =
        cc["wechat"]["oa"].get("secret", "dev-oa-secret").asString();
    if (const char* env = std::getenv("WECHAT_OA_APPID"); env && *env) {
        wechatOaAppid = env;
    }
    if (const char* env = std::getenv("WECHAT_OA_SECRET"); env && *env) {
        wechatOaSecret = env;
    }
    // 微信 API 基础地址（默认官方域名）；本地验收可指向 mock 服务。
    std::string wechatApiBaseUrl =
        cc["wechat"].get("api_base_url", "https://api.weixin.qq.com").asString();
    if (const char* env = std::getenv("WECHAT_API_BASE_URL"); env && *env) {
        wechatApiBaseUrl = env;
    }

    // 生产环境对缺失/占位密钥快速失败（设计文档 十：
    // "Secret 只放 K8s Secret"）。APP_ENV=prod 要求真实值——任何
    // 仍等于开发占位符或为空的值都视为配置错误。
    std::string appEnv = "dev";
    if (const char* env = std::getenv("APP_ENV"); env && *env) {
        appEnv = env;
    }
    if (appEnv == "prod" || appEnv == "production") {
        const auto isPlaceholder = [](const std::string& v) {
            return v.empty() || v == "dev-internal-token" ||
                   v == "dev-shared-jwt-secret" || v == "dev-miniapp-secret" ||
                   v == "dev-oa-secret" || v == "CHANGE_ME";
        };
        if (isPlaceholder(internalToken) || isPlaceholder(jwtSecret) ||
            isPlaceholder(wechatSecret) || isPlaceholder(wechatOaSecret)) {
            LOG_FATAL << "APP_ENV=prod but a required secret is empty or a "
                         "placeholder; set INTERNAL_TOKEN / JWT_SECRET / "
                         "WECHAT_SECRET / WECHAT_OA_SECRET from a K8s Secret";
            return 1;
        }
    } else if (internalToken == "dev-internal-token" ||
               jwtSecret == "dev-shared-jwt-secret") {
        LOG_WARN << "running with dev placeholder secrets; set real secrets via "
                    "env before production";
    }

    std::string goBackendUrl =
        cc["go_backend"].get("base_url", "http://127.0.0.1:8081").asString();
    if (const char* env = std::getenv("GO_BACKEND_URL"); env && *env) {
        goBackendUrl = env;
    }
    std::string goBackendApiPrefix =
        cc["go_backend"].get("api_prefix", "/api").asString();

    // go-backend 客户端提前构造：短信兜底的手机号实时查询依赖它，
    // 而渠道在下方装配（内部调用用 X-Internal-Token）。
    auto goBackendClient = std::make_shared<GoBackendClient>(goBackendUrl);
    goBackendClient->setInternalToken(internalToken);
    goBackendClient->setApiPrefix(goBackendApiPrefix);

    // Redis（设计文档 十）：支撑 wechat:token:{miniapp,oa}、刷新
    // 锁与通知冷却。host 为空 => 进程内回退（开发）。
    std::string redisHost = cc["redis"].get("host", "").asString();
    int redisPort = cc["redis"].get("port", 6379).asInt();
    std::string redisPassword = cc["redis"].get("password", "").asString();
    if (const char* env = std::getenv("REDIS_HOST"); env && *env) {
        redisHost = env;
    }
    if (const char* env = std::getenv("REDIS_PORT"); env && *env) {
        redisPort = std::atoi(env);
    }
    if (const char* env = std::getenv("REDIS_PASSWORD"); env && *env) {
        redisPassword = env;
    }

    // 通知配置（设计文档 九 9.5）：各渠道启用开关、默认
    // 冷却窗口和短信兜底开关。
    const Json::Value notifyCfg =
        cc.isMember("notify") ? cc["notify"] : Json::Value();
    bool fallbackEnabled = true;
    std::string fallbackChannel = "sms";
    int defaultCooldown = 1800;
    if (notifyCfg.isMember("fallback")) {
        fallbackEnabled = notifyCfg["fallback"].get("enabled", true).asBool();
        fallbackChannel = notifyCfg["fallback"].get("channel", "sms").asString();
    }
    if (notifyCfg.isMember("cooldown")) {
        defaultCooldown =
            notifyCfg["cooldown"].get("default_seconds", 1800).asInt();
    }
    // pending_events 后端的 Postgres 连接串（设计文档 13.4）。为空
    // -> 使用内存仓库（开发/测试）。环境变量覆盖：PG_CONNINFO。
    std::string pgConninfo;
    if (cc.isMember("postgres")) {
        pgConninfo = cc["postgres"].get("conninfo", "").asString();
    }
    if (const char* env = std::getenv("PG_CONNINFO"); env && *env) {
        pgConninfo = env;
    }

    // 运维操作日志的文件兜底路径（设计文档 7.5）。空串 = 禁用文件兜底。
    std::string operationLogFile = "operation_logs.jsonl";
    if (cc.isMember("operation_log")) {
        operationLogFile =
            cc["operation_log"].get("file", "operation_logs.jsonl").asString();
    }

    // 生产环境必须有 PostgreSQL 后端（设计文档 14）——绝不静默跑在内存上。
    if (appEnv == "prod" || appEnv == "production") {
#ifndef HAS_LIBPQ
        LOG_FATAL << "APP_ENV=prod but libpq is unavailable; install libpq-dev "
                     "and rebuild to enable the PostgreSQL backend";
        return 1;
#else
        if (pgConninfo.empty()) {
            LOG_FATAL << "APP_ENV=prod but postgres.conninfo is empty; set "
                         "PG_CONNINFO from a K8s Secret";
            return 1;
        }
#endif
    }

    // 重试工作线程调度（设计文档 13.4 / 13.9）。
    RetryConfig retryCfg;
    if (notifyCfg.isMember("retry")) {
        const Json::Value& r = notifyCfg["retry"];
        retryCfg.max_attempts = r.get("max_attempts", 5).asInt();
        retryCfg.base_delay = std::chrono::milliseconds(static_cast<long long>(
            r.get("base_delay_seconds", 60).asDouble() * 1000));
        retryCfg.max_delay = std::chrono::milliseconds(static_cast<long long>(
            r.get("max_delay_seconds", 1800).asDouble() * 1000));
        retryCfg.jitter_ratio = r.get("jitter_ratio", 0.2).asDouble();
        retryCfg.poll_interval = std::chrono::milliseconds(
            static_cast<long long>(r.get("poll_interval_seconds", 1).asDouble() * 1000));
        retryCfg.batch_size = r.get("batch_size", 50).asInt();
    }

    app().addListener("0.0.0.0", port);

    // 内部令牌鉴权过滤器——在 registerHandler 中按名称引用时由
    // Drogon 自动创建（HttpFilter<T> 的 isAutoCreation=true）。我们只需
    // 在首个请求前填充其静态令牌；无需手动 registerFilter。
    InternalTokenFilter::setToken(internalToken);

    // JWT bearer 过滤器（小程序）+ 共享密钥（同样自动创建）。
    JwtUtil::setSecret(jwtSecret);

    // PostgreSQL 连接池：配置了 conninfo 且具备 libpq 时建立；否则各仓库
    // 用内存 mock（开发/测试）。生产（APP_ENV=prod）已在上面强制非空。
    std::shared_ptr<PgPool> pgPool;
#ifdef HAS_LIBPQ
    if (!pgConninfo.empty()) {
        pgPool = std::make_shared<PgPool>(pgConninfo);
    }
#endif

    // 仓库（PG 优先、内存回退）。每个仓库先用内存实现占位，有 PG 时替换为
    // 真实 PostgreSQL 实现；开发演示数据已移到 sql/seed_dev.sql。
    std::shared_ptr<NotifyLogRepository> notifyLogRepo;
    std::shared_ptr<NotifyTargetRepository> notifyTargetRepo;
    std::shared_ptr<PendingEventRepository> pendingRepo;
    std::shared_ptr<ParentBindingRepository> bindingRepo;
    std::shared_ptr<OaNotifyLogRepository> oaNotifyLogRepo;
    std::shared_ptr<UserSpacesRepository> userSpacesRepo;
    std::shared_ptr<SpaceRepository> spaceRepo;
    std::shared_ptr<StudentParentRepository> studentParentRepo;
    std::shared_ptr<AlertRepository> alertRepo;
    std::shared_ptr<DoorDeviceRepository> doorDeviceRepo;  // 门禁白名单（7.5③）
    std::shared_ptr<SpaceTypeRepository> spaceTypeRepo;    // 空间类型（自定义类型标签）
    std::shared_ptr<SceneRepository> sceneRepo;            // 自定义场景（教师端）
    std::shared_ptr<SubscriptionRepository> subscriptionRepo;
    std::shared_ptr<WechatBindingRepository> wechatBindingRepo;
    std::shared_ptr<UserRoleRepository> userRoleRepo;
    std::shared_ptr<AccessRecordRepository> accessRecordRepo;
    std::shared_ptr<OperationLogRepository> operationLogRepo;  // 仅 Pg；无 libpq 时为 nullptr
    // 渠道地址解析（读）+ admin 绑定写。两者共用同一底层实例（InMemory 或 Pg）。
    std::shared_ptr<INotifyTargetResolver> notifyTargetResolverBase;
    std::shared_ptr<NotifyBindingRepository> notifyBindingRepo;

#ifdef HAS_LIBPQ
    if (pgPool) {
        notifyLogRepo = std::make_shared<PgNotifyLogRepository>(pgPool);
        notifyTargetRepo = std::make_shared<PgNotifyTargetRepository>(pgPool);
        const char* host = std::getenv("HOSTNAME");
        pendingRepo = std::make_shared<PgPendingEventRepository>(
            pgPool, host ? host : "localhost");
        bindingRepo = std::make_shared<PgParentBindingRepository>(pgPool);
        oaNotifyLogRepo = std::make_shared<PgOaNotifyLogRepository>(pgPool);
        userSpacesRepo = std::make_shared<PgUserSpacesRepository>(pgPool);
        spaceRepo = std::make_shared<PgSpaceRepository>(pgPool);
        spaceTypeRepo = std::make_shared<PgSpaceTypeRepository>(pgPool);
        sceneRepo = std::make_shared<PgSceneRepository>(pgPool);
        studentParentRepo = std::make_shared<PgStudentParentRepository>(pgPool);
        alertRepo = std::make_shared<PgAlertRepository>(pgPool);
        doorDeviceRepo = std::make_shared<PgDoorDeviceRepository>(pgPool);
        subscriptionRepo = std::make_shared<PgSubscriptionRepository>(pgPool);
        wechatBindingRepo = std::make_shared<PgWechatBindingRepository>(pgPool);
        {
            auto pgResolver = std::make_shared<PgNotifyTargetResolver>(pgPool);
            notifyTargetResolverBase = pgResolver;
            notifyBindingRepo = pgResolver;
        }
        userRoleRepo = std::make_shared<PgUserRoleRepository>(pgPool);
        accessRecordRepo = std::make_shared<PgAccessRecordRepository>(pgPool);
        operationLogRepo = std::make_shared<PgOperationLogRepository>(pgPool);
    } else
#endif
    {
        notifyLogRepo = std::make_shared<InMemoryNotifyLogRepository>();
        {
            // 纯内存模式的收件人演示数据（对齐 sql/seed_dev.sql 的 user_roles /
            // user_spaces）。否则 resolveRecipients 恒为空，通知事件会被跳过。
            auto targets = std::make_shared<InMemoryNotifyTargetRepository>();
            targets->addUser("u_admin_1", "admin");
            targets->addUser("u_admin_2", "admin");
            targets->addSpaceMember("spc_a8acdd5c", "u_teacher_1");
            notifyTargetRepo = targets;
        }
        pendingRepo = std::make_shared<InMemoryPendingEventRepository>();
        bindingRepo = std::make_shared<InMemoryParentBindingRepository>();
        {
            // 花名册种子（对齐 sql/seed_dev.sql），使绑定校验在纯内存模式下可演示。
            auto parentsMem = std::make_shared<InMemoryStudentParentRepository>();
            parentsMem->add({"20260101", "张三", "13800000000", "父亲", "active"});
            studentParentRepo = parentsMem;
        }
        oaNotifyLogRepo = std::make_shared<InMemoryOaNotifyLogRepository>();
        userSpacesRepo = std::make_shared<InMemoryUserSpacesRepository>();
        {
            // 空间种子（对齐 sql/seed_dev.sql），使绑定/进出记录在纯内存模式下可演示。
            auto spacesMem = std::make_shared<InMemorySpaceRepository>();
            spacesMem->add(Space{"spc_a8acdd5c", "A101 教室", "standard"});
            spacesMem->add(Space{"spc_std_a102", "A102 教室", "standard"});
            spacesMem->add(Space{"spc_b7bee44d", "多媒体报告厅", "lecture"});
            spaceRepo = spacesMem;
        }
        {
            // 空间类型种子（对齐 sql/migration_v15.sql 的 7 条），使设备页类型
            // tab 在纯内存模式下可演示。
            auto spaceTypesMem = std::make_shared<InMemorySpaceTypeRepository>();
            spaceTypesMem->add(SpaceType{"standard", "标准教室", 0, true, ""});
            spaceTypesMem->add(SpaceType{"lecture", "多媒体报告厅", 1, true, ""});
            spaceTypesMem->add(SpaceType{"office", "办公室", 2, true, ""});
            spaceTypesMem->add(SpaceType{"gym", "体育馆", 3, true, ""});
            spaceTypesMem->add(SpaceType{"lab", "实验室", 4, true, ""});
            spaceTypesMem->add(SpaceType{"library", "图书馆", 5, true, ""});
            spaceTypesMem->add(SpaceType{"canteen", "食堂", 6, true, ""});
            spaceTypeRepo = spaceTypesMem;
            sceneRepo = std::make_shared<InMemorySceneRepository>();
        }
        {
            // 操作人姓名种子（对齐 sql/seed_dev.sql 的 wechat_bindings.name）。
            // PG 模式 operator_name 靠 LEFT JOIN wechat_bindings 解析；纯内存模式无 JOIN，
            // 把姓名喂给 InMemoryAlertRepository，使 handle 后能显示操作人。
            auto alertsMem = std::make_shared<InMemoryAlertRepository>();
            alertsMem->setName("u_admin_1", "管理员");
            alertsMem->setName("u_admin_2", "管理员");
            alertsMem->setName("u_teacher_1", "李老师");
            alertsMem->setName("u_teacher_2", "王老师");
            alertRepo = alertsMem;
        }
        doorDeviceRepo = std::make_shared<InMemoryDoorDeviceRepository>();
        subscriptionRepo = std::make_shared<InMemorySubscriptionRepository>();
        wechatBindingRepo = std::make_shared<InMemoryWechatBindingRepository>();
        {
            auto memResolver = std::make_shared<InMemoryNotifyTargetResolver>();
            notifyTargetResolverBase = memResolver;
            notifyBindingRepo = memResolver;
        }
        userRoleRepo = std::make_shared<InMemoryUserRoleRepository>();
        // 教师/管理员种子（对齐 sql/seed_dev.sql 的 user_roles），使绑定管理页
        // 在纯内存模式下也能列出教师。
        userRoleRepo->setRoles("u_admin_1", {"admin"});
        userRoleRepo->setRoles("u_admin_2", {"admin"});
        userRoleRepo->setRoles("u_teacher_1", {"teacher"});
        userRoleRepo->setRoles("u_teacher_2", {"teacher"});
        // 教师-教室绑定种子（对齐 seed_dev.sql 的 user_spaces）。
        userSpacesRepo->add("u_teacher_1", "spc_a8acdd5c");
        userSpacesRepo->add("u_teacher_2", "spc_b7bee44d");
        accessRecordRepo = std::make_shared<InMemoryAccessRecordRepository>();
    }

    // 角色实时性（7.7 收尾项 d）：JwtFilter 验签后按 user_roles 覆盖 JWT
    // 内嵌 role，使角色变更无需重新登录即可生效（60s 进程内缓存）。roles
    // 为空则不覆盖，仅用 JWT 内嵌 role。
    JwtFilter::configure(userRoleRepo, /*roleCacheTtlSeconds=*/60);

    // 渠道。被禁用的渠道会被注册，但分发时跳过。小程序订阅消息 / 短信
    // 都带 mode=mock|real 开关（dev 默认 mock，防止演示环境误触真实外部服务）。
    std::shared_ptr<RedisClient> redis =
        redisHost.empty()
            ? std::make_shared<RedisClient>()
            : std::make_shared<RedisClient>(redisHost, redisPort, redisPassword);
    auto notifyService = std::make_shared<NotifyService>(
        notifyLogRepo, notifyTargetRepo, pendingRepo, redis);
    notifyService->setRules(defaultPushRules());
    notifyService->setFallbackEnabled(fallbackEnabled);
    notifyService->setFallbackChannel(fallbackChannel);
    notifyService->setDefaultCooldown(defaultCooldown);
    notifyService->setAlertRepo(alertRepo);

    // 渠道地址解析：基础解析（user_notify_bindings）+ sms 手机号 go-backend 回落。
    notifyService->setTargetResolver(std::make_shared<FallbackNotifyTargetResolver>(
        notifyTargetResolverBase, goBackendClient));

    // 小程序订阅消息模板：event_type -> template_id / data 字段占位符。
    std::map<std::string, std::string> miniappTemplateIds;
    std::map<std::string, std::map<std::string, std::string>>
        miniappTemplateFields;
    std::set<std::string> miniappTemplateIdSet;       // 去重后的模板 ID（回显/校验）
    std::set<std::string> miniappLongTermTemplateIds; // 其中 long_term 的模板 ID
    bool miniappRealMode = false;
    if (notifyCfg.isMember("channels") &&
        notifyCfg["channels"].isMember("wechat_miniapp")) {
        const Json::Value& mc = notifyCfg["channels"]["wechat_miniapp"];
        miniappRealMode = mc.get("mode", "mock").asString() == "real";
        if (mc.isMember("templates")) {
            const Json::Value& tpls = mc["templates"];
            for (const auto& key : tpls.getMemberNames()) {
                const Json::Value& t = tpls[key];
                if (t.isString()) {
                    miniappTemplateIds[key] = t.asString();
                    miniappTemplateIdSet.insert(t.asString());
                } else if (t.isObject() && t.isMember("id")) {
                    const std::string tid = t["id"].asString();
                    miniappTemplateIds[key] = tid;
                    miniappTemplateIdSet.insert(tid);
                    if (t.get("type", "one_time").asString() == "long_term") {
                        miniappLongTermTemplateIds.insert(tid);
                    }
                    if (t.isMember("fields") && t["fields"].isObject()) {
                        const Json::Value& f = t["fields"];
                        for (const auto& fk : f.getMemberNames()) {
                            miniappTemplateFields[key][fk] = f[fk].asString();
                        }
                    }
                }
            }
        }
    }
    // 模板 ID 多环境覆盖（7.7 收尾项 a）：真实模板 ID 经微信平台审核后从
    // 环境变量注入，不硬编码进仓库。只覆盖已配置的事件类型；多小程序/多租户
    // 模板配置归 7.9，此处按单小程序处理。
    const std::map<std::string, std::string> kTemplateEnv = {
        {"device_offline", "MINIAPP_TMPL_DEVICE_OFFLINE"},
        {"peripheral_offline", "MINIAPP_TMPL_PERIPHERAL_OFFLINE"},
        {"sensor_threshold", "MINIAPP_TMPL_SENSOR_THRESHOLD"},
        {"face_login_failed", "MINIAPP_TMPL_FACE_LOGIN_FAILED"},
    };
    for (const auto& [event, envName] : kTemplateEnv) {
        const char* env = std::getenv(envName.c_str());
        auto it = miniappTemplateIds.find(event);
        if (env && *env && it != miniappTemplateIds.end()) {
            const std::string tid(env);
            miniappTemplateIdSet.erase(it->second);
            miniappTemplateIdSet.insert(tid);
            it->second = tid;
        }
    }

    // 生产环境对模板 ID 快速失败：真实模式（miniappRealMode）下拒绝仍为
    // 占位符（tmpl_*）或为空的模板 ID。
    if (miniappRealMode && (appEnv == "prod" || appEnv == "production")) {
        for (const auto& [event, tid] : miniappTemplateIds) {
            if (tid.empty() || tid.rfind("tmpl_", 0) == 0) {
                LOG_FATAL
                    << "APP_ENV=prod with miniapp mode=real but template for '"
                    << event << "' is empty or a tmpl_* placeholder; set the"
                    << " real template id via env";
                return 1;
            }
        }
    }

    // 微信 access_token 缓存（设计文档 十），由小程序和 OA
    // 渠道共享。get() 仅在真实 send() 落地后由工作线程调用——
    // 不得在 Drogon 事件循环上运行。
    auto tokenManager = std::make_shared<WechatTokenManager>(
        WechatTokenManager::Credentials{wechatAppid, wechatSecret},
        WechatTokenManager::Credentials{wechatOaAppid, wechatOaSecret}, redis,
        wechatApiBaseUrl);

    // 渠道：按 config 实例化（7.9 插件化）。工厂只按名字造对象、不按 enabled
    // 跳过；enabled 由 NotifyService 发送时过滤。wechat_oa 实例复用给
    // OaNotifyService（到校推送，独立于 enabled），wechat_miniapp 实例复用给
    // SubscribeNotifyController（订阅消息最小触发接口）。
    std::shared_ptr<WechatMiniAppChannel> miniappChannel;
    ChannelDeps channelDeps{tokenManager, goBackendClient, wechatApiBaseUrl,
                            subscriptionRepo, redis};
    if (notifyCfg.isMember("channels")) {
        const Json::Value& chans = notifyCfg["channels"];
        for (const auto& name : chans.getMemberNames()) {
            ChannelConfig cfg;
            cfg.enabled = chans[name].get("enabled", true).asBool();
            cfg.raw = toNlohmann(chans[name]);
            auto channel =
                ChannelFactory::instance().build(name, cfg, channelDeps);
            if (!channel) {
                LOG_WARN << "notify: no factory builder for channel '" << name
                         << "', skipped";
                continue;
            }
            if (name == "wechat_miniapp") {
                miniappChannel =
                    std::dynamic_pointer_cast<WechatMiniAppChannel>(channel);
            }
            notifyService->registerChannel(channel);
        }
    }

    // 小程序模板配置（event -> template_id / data 字段）注入渠道；env 覆盖与
    // prod 校验已在上面完成。wechat_oa 不再经工厂（到校走独立路径）。
    if (miniappChannel) {
        miniappChannel->setTemplates(miniappTemplateIds, miniappTemplateFields);
    }

    // 配置驱动路由（7.9）：notify.routing 存在时才覆盖；缺失、或 default 为空
    // 且无按事件覆盖时不调用（保留 PushRule 代码缺省 {"wechat_miniapp"}）。
    // default 为空但有按事件覆盖时仍调用：未覆盖事件清空走短信兜底（有意语义）。
    if (notifyCfg.isMember("routing")) {
        const Json::Value& rt = notifyCfg["routing"];
        std::vector<std::string> defaultChannels;
        std::map<std::string, std::vector<std::string>> byEvent;
        bool hasDefault = false;
        if (rt.isMember("default") && rt["default"].isArray()) {
            hasDefault = !rt["default"].empty();
            for (const auto& ch : rt["default"]) {
                defaultChannels.push_back(ch.asString());
            }
        }
        bool hasOverride = false;
        for (const auto& key : rt.getMemberNames()) {
            if (key == "default" || !rt[key].isArray()) {
                continue;
            }
            hasOverride = true;
            std::vector<std::string> evChans;
            for (const auto& ch : rt[key]) {
                evChans.push_back(ch.asString());
            }
            byEvent[key] = std::move(evChans);
        }
        if (hasDefault || hasOverride) {
            notifyService->setRouting(defaultChannels, byEvent);
        }
    }

    // 重试工作线程（设计文档 13.4）：在后台重新分发失败事件。
    // 分发器解析暂存的负载，并在无冷却的情况下重新执行
    // 按用户分发（通过台账保证幂等）。
    RetryWorker retryWorker(
        pendingRepo,
        [notifyService](const nlohmann::json& payload) -> RetryOutcome {
            NotifySendRequest req;
            std::string err;
            if (!NotifySendRequest::fromJson(payload, req, err)) {
                return RetryOutcome{true, "", ""};  // 无法解析 -> 丢弃
            }
            return notifyService->redispatch(req);
        },
        retryCfg);
    retryWorker.start();

    // OA 到校通知渠道（独立路径）：凭证 wechat.oa，业务配置 notify.oa。
    std::string oaArrivalTemplateId = "tmpl_arrival";
    bool oaEnabled = true;
    bool oaRealMode = false;
    bool oaNotifyOncePerDay = true;
    bool oaNotifyOnLeave = false;
    if (notifyCfg.isMember("oa")) {
        oaArrivalTemplateId =
            notifyCfg["oa"].get("arrival_template_id", "tmpl_arrival").asString();
        oaEnabled = notifyCfg["oa"].get("enabled", true).asBool();
        oaRealMode = notifyCfg["oa"].get("mode", "mock").asString() == "real";
        oaNotifyOncePerDay =
            notifyCfg["oa"].get("notify_once_per_day", true).asBool();
        oaNotifyOnLeave = notifyCfg["oa"].get("notify_on_leave", false).asBool();
    } else if (notifyCfg.isMember("channels") &&
               notifyCfg["channels"].isMember("wechat_oa")) {
        // 旧位置 notify.channels.wechat_oa 已废弃：不再兼容读取，避免静默
        // 忽略模板 ID / enabled。配置须迁移到 notify.oa。
        LOG_FATAL << "notify.channels.wechat_oa is not supported; move "
                     "arrival_template_id/enabled to notify.oa";
        return 1;
    }
    // OA 到校模板 ID 环境变量覆盖（与小程序模板 MINIAPP_TMPL_* 对齐）：
    // 真实模板经微信审核后从 env 注入，不硬编码进仓库/镜像。
    if (const char* env = std::getenv("WECHAT_OA_TMPL_ARRIVAL"); env && *env) {
        oaArrivalTemplateId = env;
    }
    // 生产环境对 OA 到校模板快速失败：真实模式下拒绝仍为占位符（tmpl_*）
    // 或为空的模板 ID，避免「配置缺失仍上线」。
    if (oaRealMode && (appEnv == "prod" || appEnv == "production") &&
        (oaArrivalTemplateId.empty() ||
         oaArrivalTemplateId.rfind("tmpl_", 0) == 0)) {
        LOG_FATAL << "APP_ENV=prod with notify.oa.mode=real but "
                     "arrival_template_id is empty or a tmpl_* placeholder; set "
                     "the real template id via WECHAT_OA_TMPL_ARRIVAL";
        return 1;
    }
    auto oaChannel = std::make_shared<WechatOaChannel>(
        tokenManager, wechatApiBaseUrl, oaArrivalTemplateId, oaEnabled,
        oaRealMode);
    auto oaService = std::make_shared<OaNotifyService>(
        bindingRepo, oaChannel, oaNotifyLogRepo, oaNotifyOncePerDay,
        oaNotifyOnLeave);

    // 启动时打印各 notify 渠道的启用/模式（健康检查与排障用）。
    // disabled = 未配置或 enabled=false；mock = 显式 dry-run（发送时还会打日志）。
    const auto channelModeOf = [&](const std::string& name) -> std::string {
        if (!notifyCfg.isMember("channels") ||
            !notifyCfg["channels"].isMember(name)) {
            return "disabled";
        }
        const Json::Value& c = notifyCfg["channels"][name];
        if (!c.get("enabled", true).asBool()) {
            return "disabled";
        }
        return c.get("mode", "mock").asString();
    };
    const std::string smsMode = channelModeOf("sms");
    const std::string dingtalkMode = channelModeOf("dingtalk");
    const std::string wecomMode = channelModeOf("wecom");
    const std::string miniappModeStr = miniappRealMode ? "real" : "mock";
    const std::string oaModeStr = oaEnabled ? (oaRealMode ? "real" : "mock")
                                            : "disabled";
    nlohmann::json channelStatus = nlohmann::json::object();
    channelStatus["wechat_miniapp"] = miniappModeStr;
    channelStatus["wechat_oa"] = oaModeStr;
    channelStatus["sms"] = smsMode;
    channelStatus["dingtalk"] = dingtalkMode;
    channelStatus["wecom"] = wecomMode;
    LOG_INFO << "notify channels: wechat_miniapp=" << miniappModeStr
             << " wechat_oa=" << oaModeStr << " sms=" << smsMode
             << " dingtalk=" << dingtalkMode << " wecom=" << wecomMode;

    // 小程序鉴权（六节）。同一客户端还承载 OA 网页鉴权
    // 凭证，用于家长到校通知绑定流程（设计文档 十一）。
    auto wechatClient = std::make_shared<WechatClient>(
        wechatAppid, wechatSecret, wechatOaAppid, wechatOaSecret);
    auto authService = std::make_shared<AuthService>(
        wechatClient, goBackendClient, wechatBindingRepo, accessTtl, openidTtl);

    // 家长到校通知绑定流程（设计文档 十一 绑定流程）。
    // 短信验证码：notify.oa.sms.mode=real 时复用 notify.channels.sms 的
    // provider / 签名 / 凭证（密钥只走 SMS_ACCESS_KEY / SMS_SECRET 环境变量）
    // 真发验证码短信；否则退回内存 mock（开发/测试）。notify.oa.sms 缺失时
    // 默认 mock，不抛异常。
    std::shared_ptr<SmsClient> smsClient = std::make_shared<SmsClient>();
    if (notifyCfg.isMember("oa") && notifyCfg["oa"].isMember("sms") &&
        notifyCfg["oa"]["sms"].get("mode", "mock").asString() == "real") {
        const Json::Value& oaSms = notifyCfg["oa"]["sms"];
        const Json::Value smsCh = notifyCfg["channels"].isMember("sms")
                                      ? notifyCfg["channels"]["sms"]
                                      : Json::Value();
        const std::string provider =
            smsCh.get("provider", "aliyun").asString();
        const std::string sign = smsCh.get("sign_name", "").asString();
        const std::string accessKey = smsCh.get("access_key", "").asString();
        const std::string secret = smsCh.get("secret", "").asString();
        const std::string region =
            smsCh.get("region", "cn-hangzhou").asString();
        const std::string sdkAppId = smsCh.get("sdk_app_id", "").asString();
        const std::string templateCode = oaSms.get("template_code", "").asString();
        const std::string codeParam = oaSms.get("code_param", "code").asString();

        auto providerPtr =
            sms::makeProvider(provider, accessKey, secret, region, sdkAppId);
        smsClient = std::make_shared<SmsClient>(providerPtr, /*realMode=*/true,
                                                sign, templateCode, codeParam);
    }
    auto oaBindService = std::make_shared<OaBindService>(
        wechatClient, smsClient, bindingRepo, studentParentRepo);
    auto deviceControlService = std::make_shared<DeviceControlService>(
        goBackendClient, userSpacesRepo, doorDeviceRepo, spaceRepo);
    auto sceneService = std::make_shared<SceneService>(
        goBackendClient, userSpacesRepo, spaceRepo, sceneRepo,
        operationLogRepo, operationLogFile);
    auto deviceService =
        std::make_shared<DeviceService>(goBackendClient, userSpacesRepo);
    auto spaceService =
        std::make_shared<SpaceService>(spaceRepo, userSpacesRepo, spaceTypeRepo);
    auto alertService = std::make_shared<AlertService>(
        alertRepo, userSpacesRepo, operationLogRepo, operationLogFile);
    auto subscriptionService = std::make_shared<SubscriptionService>(
        subscriptionRepo, miniappTemplateIdSet, miniappLongTermTemplateIds);
    auto spaceBindingService = std::make_shared<SpaceBindingService>(
        userSpacesRepo, userRoleRepo, spaceRepo, wechatBindingRepo);
    auto spaceSyncService = std::make_shared<SpaceSyncService>(
        spaceRepo, userSpacesRepo, doorDeviceRepo);
    auto studentParentSyncService =
        std::make_shared<StudentParentSyncService>(studentParentRepo);
    auto accessRecordService = std::make_shared<AccessRecordService>(
        accessRecordRepo, userSpacesRepo);
    auto operationLogService =
        std::make_shared<OperationLogService>(operationLogRepo);
    auto doorDeviceService = std::make_shared<DoorDeviceService>(
        doorDeviceRepo, userSpacesRepo, operationLogRepo, operationLogFile);
    auto spaceTypeService = std::make_shared<SpaceTypeService>(spaceTypeRepo);
    auto notifyBindingService =
        std::make_shared<NotifyBindingService>(notifyBindingRepo);

    // 控制器。
    auto notifyCtrl = std::make_shared<NotifyController>(notifyService);
    auto oaCtrl = std::make_shared<OaController>(oaService, oaBindService);
    auto miniAppCtrl = std::make_shared<MiniAppController>(
        authService, deviceControlService);
    auto sceneCtrl = std::make_shared<SceneController>(sceneService);
    auto deviceCtrl =
        std::make_shared<DeviceController>(deviceService);
    auto spaceCtrl = std::make_shared<SpaceController>(spaceService);
    auto alertCtrl = std::make_shared<AlertController>(alertService);
    auto subscriptionCtrl =
        std::make_shared<SubscriptionController>(subscriptionService);
    auto spaceBindingCtrl =
        std::make_shared<SpaceBindingController>(spaceBindingService);
    auto accessRecordCtrl =
        std::make_shared<AccessRecordController>(accessRecordService);
    auto operationLogCtrl =
        std::make_shared<OperationLogController>(operationLogService);
    auto doorDeviceCtrl =
        std::make_shared<DoorDeviceController>(doorDeviceService);
    auto spaceTypeCtrl =
        std::make_shared<SpaceTypeController>(spaceTypeService);
    auto subscribeNotifyCtrl =
        std::make_shared<SubscribeNotifyController>(miniappChannel);
    auto userRoleCtrl = std::make_shared<UserRoleController>(userRoleRepo);
    auto spaceSyncCtrl =
        std::make_shared<SpaceSyncController>(spaceSyncService);
    auto studentParentSyncCtrl =
        std::make_shared<StudentParentSyncController>(studentParentSyncService);
    auto notifyBindingCtrl =
        std::make_shared<NotifyBindingController>(notifyBindingService);

    // WebSocket 代理控制器依赖注入（边侧 /ws 转发，设计文档 7.4②）。
    // 控制器本身由 Drogon 按类名自动创建（WebSocketController 自动注册），
    // 这里只喂给它边侧地址与空间仓库。
    WsProxyController::configure(goBackendUrl, userSpacesRepo);

    // 路由。

    // 健康检查（公开、无鉴权）：报告 DB / Redis / 各 notify 渠道状态。
    // 进程存活恒 200（K8s livenessProbe 语义），组件状态放 body 供 readiness /
    // 排障读取。db/redis 未配置（内存回退）标 "in-memory"，配置了但不可达标
    // "unavailable"，此时整体 status 降为 "degraded"。
    app().registerHandler(
        "/health",
        [pgPool, redis, channelStatus, redisConfigured = !redisHost.empty()](
            const HttpRequestPtr&,
            std::function<void(const HttpResponsePtr&)>&& callback) {
            nlohmann::json db = "in-memory";
            if (pgPool) {
                auto* res = pgPool->exec("SELECT 1");
                const bool ok = (res != nullptr);
                if (res) {
                    pgPool->clear(res);
                }
                db = ok ? "ok" : "unavailable";
            }
            const nlohmann::json redisStatus =
                redisConfigured ? (redis->ping() ? "ok" : "unavailable")
                                : nlohmann::json("in-memory");

            nlohmann::json body = nlohmann::json::object();
            body["service"] = "wechat-gateway";
            body["db"] = db;
            body["redis"] = redisStatus;
            body["channels"] = channelStatus;
            body["status"] = (db == "unavailable" || redisStatus == "unavailable")
                                 ? "degraded"
                                 : "ok";

            auto resp = HttpResponse::newHttpResponse();
            resp->setContentTypeCode(CT_APPLICATION_JSON);
            resp->setBody(body.dump());
            callback(resp);
        },
        {Get});

    app().registerHandler(
        "/internal/notify/send",
        [notifyCtrl](const HttpRequestPtr& req,
                     std::function<void(const HttpResponsePtr&)>&& cb) {
            notifyCtrl->send(req, std::move(cb));
        },
        {Post, std::string("InternalTokenFilter")});

    app().registerHandler(
        "/internal/notify/subscribe-send",
        [subscribeNotifyCtrl](const HttpRequestPtr& req,
                              std::function<void(const HttpResponsePtr&)>&& cb) {
            subscribeNotifyCtrl->send(req, std::move(cb));
        },
        {Post, std::string("InternalTokenFilter")});

    app().registerHandler(
        "/internal/oa/arrival-notify",
        [oaCtrl](const HttpRequestPtr& req,
                 std::function<void(const HttpResponsePtr&)>&& cb) {
            oaCtrl->arrivalNotify(req, std::move(cb));
        },
        {Post, std::string("InternalTokenFilter")});

    app().registerHandler(
        "/internal/user-roles/sync",
        [userRoleCtrl](const HttpRequestPtr& req,
                       std::function<void(const HttpResponsePtr&)>&& cb) {
            userRoleCtrl->sync(req, std::move(cb));
        },
        {Post, std::string("InternalTokenFilter")});

    app().registerHandler(
        "/internal/access-records",
        [accessRecordCtrl](const HttpRequestPtr& req,
                           std::function<void(const HttpResponsePtr&)>&& cb) {
            accessRecordCtrl->ingest(req, std::move(cb));
        },
        {Post, std::string("InternalTokenFilter")});

    app().registerHandler(
        "/internal/spaces/sync",
        [spaceSyncCtrl](const HttpRequestPtr& req,
                        std::function<void(const HttpResponsePtr&)>&& cb) {
            spaceSyncCtrl->sync(req, std::move(cb));
        },
        {Post, std::string("InternalTokenFilter")});

    app().registerHandler(
        "/internal/student-parents/sync",
        [studentParentSyncCtrl](const HttpRequestPtr& req,
                                std::function<void(const HttpResponsePtr&)>&& cb) {
            studentParentSyncCtrl->sync(req, std::move(cb));
        },
        {Post, std::string("InternalTokenFilter")});

    app().registerHandler(
        "/api/oa/bind/authorize",
        [oaCtrl](const HttpRequestPtr& req,
                 std::function<void(const HttpResponsePtr&)>&& cb) {
            oaCtrl->bindAuthorize(req, std::move(cb));
        },
        {Get});

    app().registerHandler(
        "/api/oa/bind/confirm",
        [oaCtrl](const HttpRequestPtr& req,
                 std::function<void(const HttpResponsePtr&)>&& cb) {
            oaCtrl->bindConfirm(req, std::move(cb));
        },
        {Post});

    app().registerHandler(
        "/api/oa/bind/send-code",
        [oaCtrl](const HttpRequestPtr& req,
                 std::function<void(const HttpResponsePtr&)>&& cb) {
            oaCtrl->bindSendCode(req, std::move(cb));
        },
        {Post});

    app().registerHandler(
        "/api/oa/bind/me",
        [oaCtrl](const HttpRequestPtr& req,
                 std::function<void(const HttpResponsePtr&)>&& cb) {
            oaCtrl->bindMe(req, std::move(cb));
        },
        {Get});

    app().registerHandler(
        "/api/oa/bind/unbind",
        [oaCtrl](const HttpRequestPtr& req,
                 std::function<void(const HttpResponsePtr&)>&& cb) {
            oaCtrl->bindUnbind(req, std::move(cb));
        },
        {Post});

    app().registerHandler(
        "/api/miniapp/login",
        [miniAppCtrl](const HttpRequestPtr& req,
                      std::function<void(const HttpResponsePtr&)>&& cb) {
            miniAppCtrl->login(req, std::move(cb));
        },
        {Post});

    app().registerHandler(
        "/api/miniapp/bind",
        [miniAppCtrl](const HttpRequestPtr& req,
                      std::function<void(const HttpResponsePtr&)>&& cb) {
            miniAppCtrl->bind(req, std::move(cb));
        },
        {Post});

    app().registerHandler(
        "/api/miniapp/me",
        [miniAppCtrl](const HttpRequestPtr& req,
                      std::function<void(const HttpResponsePtr&)>&& cb) {
            miniAppCtrl->me(req, std::move(cb));
        },
        {Get, std::string("JwtFilter")});

    app().registerHandler(
        "/api/miniapp/device/control",
        [miniAppCtrl](const HttpRequestPtr& req,
                      std::function<void(const HttpResponsePtr&)>&& cb) {
            miniAppCtrl->deviceControl(req, std::move(cb));
        },
        {Post, std::string("JwtFilter")});

    app().registerHandler(
        "/api/miniapp/scenes",
        [sceneCtrl](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& cb) {
            sceneCtrl->list(req, std::move(cb));
        },
        {Get, std::string("JwtFilter")});

    app().registerHandler(
        "/api/miniapp/scenes",
        [sceneCtrl](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& cb) {
            sceneCtrl->create(req, std::move(cb));
        },
        {Post, std::string("JwtFilter")});

    app().registerHandler(
        "/api/miniapp/scenes/{scene_id}",
        [sceneCtrl](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& cb) {
            sceneCtrl->update(req, std::move(cb));
        },
        {Put, std::string("JwtFilter")});

    app().registerHandler(
        "/api/miniapp/scenes/{scene_id}",
        [sceneCtrl](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& cb) {
            sceneCtrl->remove(req, std::move(cb));
        },
        {Delete, std::string("JwtFilter")});

    app().registerHandler(
        "/api/miniapp/scenes/{scene_id}/execute",
        [sceneCtrl](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& cb) {
            sceneCtrl->execute(req, std::move(cb));
        },
        {Post, std::string("JwtFilter")});

    app().registerHandler(
        "/api/miniapp/devices",
        [deviceCtrl](const HttpRequestPtr& req,
                     std::function<void(const HttpResponsePtr&)>&& cb) {
            deviceCtrl->devices(req, std::move(cb));
        },
        {Get, std::string("JwtFilter")});

    app().registerHandler(
        "/api/miniapp/spaces",
        [spaceCtrl](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& cb) {
            spaceCtrl->spaces(req, std::move(cb));
        },
        {Get, std::string("JwtFilter")});

    app().registerHandler(
        "/api/admin/spaces",
        [spaceCtrl](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& cb) {
            spaceCtrl->adminList(req, std::move(cb));
        },
        {Get, std::string("JwtFilter")});

    app().registerHandler(
        "/api/admin/spaces",
        [spaceCtrl](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& cb) {
            spaceCtrl->create(req, std::move(cb));
        },
        {Post, std::string("JwtFilter")});

    app().registerHandler(
        "/api/admin/spaces/{space_id}",
        [spaceCtrl](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& cb) {
            spaceCtrl->update(req, std::move(cb));
        },
        {Put, std::string("JwtFilter")});

    app().registerHandler(
        "/api/admin/spaces/{space_id}",
        [spaceCtrl](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& cb) {
            spaceCtrl->disable(req, std::move(cb));
        },
        {Delete, std::string("JwtFilter")});

    app().registerHandler(
        "/api/miniapp/alerts",
        [alertCtrl](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& cb) {
            alertCtrl->list(req, std::move(cb));
        },
        {Get, std::string("JwtFilter")});

    app().registerHandler(
        "/api/miniapp/alerts/{id}",
        [alertCtrl](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& cb) {
            alertCtrl->detail(req, std::move(cb));
        },
        {Get, std::string("JwtFilter")});

    app().registerHandler(
        "/api/miniapp/alerts/{id}/handle",
        [alertCtrl](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& cb) {
            alertCtrl->handle(req, std::move(cb));
        },
        {Post, std::string("JwtFilter")});

    app().registerHandler(
        "/api/miniapp/alerts/{id}/timeline",
        [alertCtrl](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& cb) {
            alertCtrl->timeline(req, std::move(cb));
        },
        {Get, std::string("JwtFilter")});

    app().registerHandler(
        "/api/miniapp/alerts/stats",
        [alertCtrl](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& cb) {
            alertCtrl->stats(req, std::move(cb));
        },
        {Get, std::string("JwtFilter")});

    app().registerHandler(
        "/api/miniapp/subscribe",
        [subscriptionCtrl](const HttpRequestPtr& req,
                           std::function<void(const HttpResponsePtr&)>&& cb) {
            subscriptionCtrl->subscribe(req, std::move(cb));
        },
        {Post, std::string("JwtFilter")});

    app().registerHandler(
        "/api/miniapp/subscribe",
        [subscriptionCtrl](const HttpRequestPtr& req,
                           std::function<void(const HttpResponsePtr&)>&& cb) {
            subscriptionCtrl->get(req, std::move(cb));
        },
        {Get, std::string("JwtFilter")});

    app().registerHandler(
        "/api/miniapp/notify/templates",
        [subscriptionCtrl](const HttpRequestPtr& req,
                           std::function<void(const HttpResponsePtr&)>&& cb) {
            subscriptionCtrl->templates(req, std::move(cb));
        },
        {Get, std::string("JwtFilter")});

    app().registerHandler(
        "/api/miniapp/space-bindings",
        [spaceBindingCtrl](const HttpRequestPtr& req,
                           std::function<void(const HttpResponsePtr&)>&& cb) {
            spaceBindingCtrl->list(req, std::move(cb));
        },
        {Get, std::string("JwtFilter")});

    app().registerHandler(
        "/api/miniapp/space-bindings",
        [spaceBindingCtrl](const HttpRequestPtr& req,
                           std::function<void(const HttpResponsePtr&)>&& cb) {
            spaceBindingCtrl->bind(req, std::move(cb));
        },
        {Post, std::string("JwtFilter")});

    app().registerHandler(
        "/api/miniapp/space-bindings",
        [spaceBindingCtrl](const HttpRequestPtr& req,
                           std::function<void(const HttpResponsePtr&)>&& cb) {
            spaceBindingCtrl->unbind(req, std::move(cb));
        },
        {Delete, std::string("JwtFilter")});

    app().registerHandler(
        "/api/notify/bindings",
        [notifyBindingCtrl](const HttpRequestPtr& req,
                            std::function<void(const HttpResponsePtr&)>&& cb) {
            notifyBindingCtrl->list(req, std::move(cb));
        },
        {Get, std::string("JwtFilter")});

    app().registerHandler(
        "/api/notify/bindings",
        [notifyBindingCtrl](const HttpRequestPtr& req,
                            std::function<void(const HttpResponsePtr&)>&& cb) {
            notifyBindingCtrl->save(req, std::move(cb));
        },
        {Post, std::string("JwtFilter")});

    app().registerHandler(
        "/api/notify/bindings",
        [notifyBindingCtrl](const HttpRequestPtr& req,
                            std::function<void(const HttpResponsePtr&)>&& cb) {
            notifyBindingCtrl->remove(req, std::move(cb));
        },
        {Delete, std::string("JwtFilter")});

    app().registerHandler(
        "/api/miniapp/access-records",
        [accessRecordCtrl](const HttpRequestPtr& req,
                           std::function<void(const HttpResponsePtr&)>&& cb) {
            accessRecordCtrl->list(req, std::move(cb));
        },
        {Get, std::string("JwtFilter")});

    app().registerHandler(
        "/api/miniapp/operation-logs",
        [operationLogCtrl](const HttpRequestPtr& req,
                           std::function<void(const HttpResponsePtr&)>&& cb) {
            operationLogCtrl->list(req, std::move(cb));
        },
        {Get, std::string("JwtFilter")});

    app().registerHandler(
        "/api/miniapp/door-devices",
        [doorDeviceCtrl](const HttpRequestPtr& req,
                         std::function<void(const HttpResponsePtr&)>&& cb) {
            doorDeviceCtrl->list(req, std::move(cb));
        },
        {Get, std::string("JwtFilter")});

    app().registerHandler(
        "/api/miniapp/door-devices",
        [doorDeviceCtrl](const HttpRequestPtr& req,
                         std::function<void(const HttpResponsePtr&)>&& cb) {
            doorDeviceCtrl->mark(req, std::move(cb));
        },
        {Post, std::string("JwtFilter")});

    app().registerHandler(
        "/api/miniapp/door-devices",
        [doorDeviceCtrl](const HttpRequestPtr& req,
                         std::function<void(const HttpResponsePtr&)>&& cb) {
            doorDeviceCtrl->unmark(req, std::move(cb));
        },
        {Delete, std::string("JwtFilter")});

    app().registerHandler(
        "/api/miniapp/space-types",
        [spaceTypeCtrl](const HttpRequestPtr& req,
                        std::function<void(const HttpResponsePtr&)>&& cb) {
            spaceTypeCtrl->list(req, std::move(cb));
        },
        {Get, std::string("JwtFilter")});

    app().registerHandler(
        "/api/admin/space-types",
        [spaceTypeCtrl](const HttpRequestPtr& req,
                        std::function<void(const HttpResponsePtr&)>&& cb) {
            spaceTypeCtrl->create(req, std::move(cb));
        },
        {Post, std::string("JwtFilter")});

    app().registerHandler(
        "/api/admin/space-types/{code}",
        [spaceTypeCtrl](const HttpRequestPtr& req,
                        std::function<void(const HttpResponsePtr&)>&& cb) {
            spaceTypeCtrl->update(req, std::move(cb));
        },
        {Put, std::string("JwtFilter")});

    app().registerHandler(
        "/api/admin/space-types/{code}",
        [spaceTypeCtrl](const HttpRequestPtr& req,
                        std::function<void(const HttpResponsePtr&)>&& cb) {
            spaceTypeCtrl->disable(req, std::move(cb));
        },
        {Delete, std::string("JwtFilter")});

    app().registerHandler(
        "/api/admin/space-types/order",
        [spaceTypeCtrl](const HttpRequestPtr& req,
                        std::function<void(const HttpResponsePtr&)>&& cb) {
            spaceTypeCtrl->reorder(req, std::move(cb));
        },
        {Patch, std::string("JwtFilter")});

    LOG_INFO << "wechat-gateway listening on 0.0.0.0:" << port;
    app().run();
    retryWorker.stop();  // 优雅关闭：发信号 + 汇合工作线程
    return 0;
}
