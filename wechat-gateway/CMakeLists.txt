cmake_minimum_required(VERSION 3.16)
project(wechat-gateway CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# nlohmann/json (header-only) via FetchContent — no system install needed.
include(FetchContent)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

# Drogon — install it first (see scripts/setup-deps.sh) and pass
# -DCMAKE_PREFIX_PATH=third_party/install.
find_package(Drogon REQUIRED)

# OpenSSL — used directly by JwtUtil for HMAC-SHA256 (base64url is hand-rolled).
find_package(OpenSSL REQUIRED)

# hiredis — real Redis backend for RedisClient (design doc 十).
find_library(HIREDIS_LIBRARY hiredis)

# libpq — optional PostgreSQL backend for pending_events + the retry worker
# (design doc 13.4). Absent without libpq-dev; the in-memory repository still
# backs dev and tests. `apt-get install libpq-dev` enables the PostgreSQL path.
find_package(PostgreSQL)
if (PostgreSQL_FOUND)
    add_definitions(-DHAS_LIBPQ)
    set(PQ_SOURCES
        src/db/src/PgPool.cpp
        src/db/src/PgAccessRecordRepository.cpp
        src/db/src/PgPendingEventRepository.cpp
        src/db/src/PgSpaceRepository.cpp
        src/db/src/PgAlertRepository.cpp
        src/db/src/PgDoorDeviceRepository.cpp
        src/db/src/PgNotifyLogRepository.cpp
        src/db/src/PgNotifyTargetRepository.cpp
        src/db/src/PgNotifyTargetResolver.cpp
        src/db/src/PgOperationLogRepository.cpp
        src/db/src/PgOaNotifyLogRepository.cpp
        src/db/src/PgParentBindingRepository.cpp
        src/db/src/PgStudentParentRepository.cpp
        src/db/src/PgSubscriptionRepository.cpp
        src/db/src/PgUserSpacesRepository.cpp
        src/db/src/PgWechatBindingRepository.cpp
        src/db/src/PgUserRoleRepository.cpp
    )
else()
    set(PQ_SOURCES "")
    message(STATUS "libpq not found — PostgreSQL pending_events backend disabled "
                   "(install libpq-dev to enable)")
endif()

# Headers now live under src/<module>/include/<module>/; add each module's
# include/ dir so the existing "#include \"module/header.h\"" style keeps working.
set(GATEWAY_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/clients/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/db/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/controllers/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/channels/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/filters/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dto/include
)

# Explicit source list (skeleton's empty .cpp files are intentionally excluded).
add_executable(wechat-gateway
    src/main.cpp
    src/controllers/src/NotifyController.cpp
    src/controllers/src/OaController.cpp
    src/controllers/src/OperationLogController.cpp
    src/controllers/src/MiniAppController.cpp
    src/controllers/src/DeviceController.cpp
    src/controllers/src/AccessRecordController.cpp
    src/controllers/src/SpaceBindingController.cpp
    src/controllers/src/NotifyBindingController.cpp
    src/controllers/src/SpaceController.cpp
    src/controllers/src/SpaceSyncController.cpp
    src/controllers/src/StudentParentSyncController.cpp
    src/controllers/src/SubscribeNotifyController.cpp
    src/controllers/src/AlertController.cpp
    src/controllers/src/DoorDeviceController.cpp
    src/controllers/src/SubscriptionController.cpp
    src/controllers/src/UserRoleController.cpp
    src/controllers/src/WsProxyController.cpp
    src/services/src/NotifyService.cpp
    src/services/src/OaNotifyService.cpp
    src/services/src/OaBindService.cpp
    src/services/src/OperationLogService.cpp
    src/services/src/AuthService.cpp
    src/services/src/DeviceControlService.cpp
    src/services/src/SceneService.cpp
    src/services/src/DeviceService.cpp
    src/services/src/AccessRecordService.cpp
    src/services/src/SpaceBindingService.cpp
    src/services/src/NotifyBindingService.cpp
    src/services/src/SpaceService.cpp
    src/services/src/SpaceSyncService.cpp
    src/services/src/StudentParentSyncService.cpp
    src/services/src/AlertService.cpp
    src/services/src/DoorDeviceService.cpp
    src/services/src/SubscriptionService.cpp
    src/services/src/RetryWorker.cpp
    src/channels/src/ChannelFactory.cpp
    src/channels/src/WechatMiniAppChannel.cpp
    src/channels/src/SmsChannel.cpp
    src/channels/src/WechatOaChannel.cpp
    src/channels/src/DingTalkChannel.cpp
    src/channels/src/WeComChannel.cpp
    src/clients/src/WechatClient.cpp
    src/clients/src/WechatTokenManager.cpp
    src/clients/src/GoBackendClient.cpp
    src/clients/src/SmsClient.cpp
    src/clients/src/AliyunSmsProvider.cpp
    src/clients/src/TencentSmsProvider.cpp
    src/clients/src/SmsProviderFactory.cpp
    src/db/src/RedisClient.cpp
    src/db/src/PendingEventRepository.cpp
    src/filters/src/InternalTokenFilter.cpp
    src/filters/src/JwtFilter.cpp
    src/utils/src/JwtUtil.cpp
    src/utils/src/Cooldown.cpp
    ${PQ_SOURCES}
)

target_include_directories(wechat-gateway PRIVATE ${GATEWAY_INCLUDE_DIRS})

target_link_libraries(wechat-gateway PRIVATE
    Drogon::Drogon
    nlohmann_json::nlohmann_json
    OpenSSL::Crypto
    ${HIREDIS_LIBRARY}
)

# libpq is optional — only link/includes when PostgreSQL was found.
if (PostgreSQL_FOUND)
    target_link_libraries(wechat-gateway PRIVATE ${PostgreSQL_LIBRARIES})
    target_include_directories(wechat-gateway PRIVATE ${PostgreSQL_INCLUDE_DIRS})
endif()

# Retry-worker unit tests — in-memory repository, no libpq / Postgres required.
enable_testing()
add_executable(retry_test
    tests/test_retry.cpp
    src/services/src/RetryWorker.cpp
    src/db/src/PendingEventRepository.cpp
)
target_include_directories(retry_test PRIVATE ${GATEWAY_INCLUDE_DIRS})
target_link_libraries(retry_test PRIVATE Drogon::Drogon nlohmann_json::nlohmann_json)
add_test(NAME retry_test COMMAND retry_test)

# 微信订阅消息渠道单测 —— 注入脚本化 transporter + 内存 Redis 预置 token，
# 无需真实微信 / Redis / Postgres。
add_executable(miniapp_channel_test
    tests/test_miniapp_channel.cpp
    src/channels/src/ChannelFactory.cpp
    src/channels/src/WechatMiniAppChannel.cpp
    src/clients/src/WechatTokenManager.cpp
    src/db/src/RedisClient.cpp
)
target_include_directories(miniapp_channel_test PRIVATE ${GATEWAY_INCLUDE_DIRS})
target_link_libraries(miniapp_channel_test PRIVATE
    Drogon::Drogon nlohmann_json::nlohmann_json ${HIREDIS_LIBRARY})
add_test(NAME miniapp_channel_test COMMAND miniapp_channel_test)

# 短信渠道单测 —— 注入脚本化 provider + 手机号解析器，无真实网络/短信。
add_executable(sms_channel_test
    tests/test_sms_channel.cpp
    src/channels/src/ChannelFactory.cpp
    src/channels/src/SmsChannel.cpp
    src/clients/src/AliyunSmsProvider.cpp
    src/clients/src/TencentSmsProvider.cpp
    src/clients/src/SmsProviderFactory.cpp
    src/clients/src/GoBackendClient.cpp
)
target_include_directories(sms_channel_test PRIVATE ${GATEWAY_INCLUDE_DIRS})
target_link_libraries(sms_channel_test PRIVATE
    Drogon::Drogon nlohmann_json::nlohmann_json OpenSSL::Crypto)
add_test(NAME sms_channel_test COMMAND sms_channel_test)

# 短信验证码客户端单测 —— mock/real 发送、provider 失败、无 provider，注入脚本化
# provider，无真实网络/短信。
add_executable(sms_client_test
    tests/test_sms_client.cpp
    src/clients/src/SmsClient.cpp
)
target_include_directories(sms_client_test PRIVATE ${GATEWAY_INCLUDE_DIRS})
target_link_libraries(sms_client_test PRIVATE nlohmann_json::nlohmann_json)
add_test(NAME sms_client_test COMMAND sms_client_test)

# 渠道工厂单测 —— 所有渠道静态自注册后，验证按名 build 返回正确类型 /
# 未知名返回 nullptr。空 ChannelDeps{} 只验类型，不触发发送。
add_executable(channel_factory_test
    tests/test_channel_factory.cpp
    src/channels/src/ChannelFactory.cpp
    src/channels/src/WechatMiniAppChannel.cpp
    src/channels/src/SmsChannel.cpp
    src/channels/src/DingTalkChannel.cpp
    src/channels/src/WeComChannel.cpp
    src/clients/src/WechatTokenManager.cpp
    src/clients/src/GoBackendClient.cpp
    src/clients/src/AliyunSmsProvider.cpp
    src/clients/src/TencentSmsProvider.cpp
    src/clients/src/SmsProviderFactory.cpp
    src/db/src/RedisClient.cpp
)
target_include_directories(channel_factory_test PRIVATE ${GATEWAY_INCLUDE_DIRS})
target_link_libraries(channel_factory_test PRIVATE
    Drogon::Drogon nlohmann_json::nlohmann_json ${HIREDIS_LIBRARY} OpenSSL::Crypto)
add_test(NAME channel_factory_test COMMAND channel_factory_test)

# 订阅仓库单测 —— 模板级额度模型，纯内存实现（无 libpq/Redis 依赖）。
add_executable(subscription_repo_test
    tests/test_subscription_repo.cpp
)
target_include_directories(subscription_repo_test PRIVATE ${GATEWAY_INCLUDE_DIRS})
add_test(NAME subscription_repo_test COMMAND subscription_repo_test)

# 场景控制单测 —— 规则匹配 + 场景目录 + 运维日志文件兜底（纯头文件实现）。
add_executable(scene_test
    tests/test_scene.cpp
)
target_include_directories(scene_test PRIVATE ${GATEWAY_INCLUDE_DIRS})
target_link_libraries(scene_test PRIVATE Drogon::Drogon nlohmann_json::nlohmann_json)
add_test(NAME scene_test COMMAND scene_test)

# 到校通知 event_id 技术幂等单测 —— 内存仓库 + 空渠道，无真实微信/Postgres/Redis。
add_executable(oa_notify_test
    tests/test_oa_notify.cpp
    src/services/src/OaNotifyService.cpp
    src/channels/src/WechatOaChannel.cpp
    src/clients/src/WechatTokenManager.cpp
    src/db/src/RedisClient.cpp
)
target_include_directories(oa_notify_test PRIVATE ${GATEWAY_INCLUDE_DIRS})
target_link_libraries(oa_notify_test PRIVATE
    Drogon::Drogon nlohmann_json::nlohmann_json ${HIREDIS_LIBRARY})
add_test(NAME oa_notify_test COMMAND oa_notify_test)

# 学生花名册单测 —— 内存仓库 + 同步全量对账 + OaBindService 绑定花名册校验。
# 用 JwtUtil 自签 openid_ticket、sms/wechat 传 null 跳过对应分支，无真实网络/Redis/Postgres。
add_executable(student_parent_test
    tests/test_student_parent.cpp
    src/services/src/OaBindService.cpp
    src/services/src/StudentParentSyncService.cpp
    src/utils/src/JwtUtil.cpp
    src/clients/src/SmsClient.cpp
    src/clients/src/WechatClient.cpp
)
target_include_directories(student_parent_test PRIVATE ${GATEWAY_INCLUDE_DIRS})
target_link_libraries(student_parent_test PRIVATE
    Drogon::Drogon nlohmann_json::nlohmann_json OpenSSL::Crypto)
add_test(NAME student_parent_test COMMAND student_parent_test)

# 家长绑定管理单测 —— me/unbind 的列表/删除语义 + token 校验（纯内存，无真实微信/Postgres）。
add_executable(oa_bind_test
    tests/test_oa_bind.cpp
    src/services/src/OaBindService.cpp
    src/utils/src/JwtUtil.cpp
    src/clients/src/SmsClient.cpp
    src/clients/src/WechatClient.cpp
)
target_include_directories(oa_bind_test PRIVATE ${GATEWAY_INCLUDE_DIRS})
target_link_libraries(oa_bind_test PRIVATE
    Drogon::Drogon nlohmann_json::nlohmann_json OpenSSL::Crypto)
add_test(NAME oa_bind_test COMMAND oa_bind_test)

# 7.6 推送规则引擎单测 —— 内存仓库 + 进程内 RedisClient + 假渠道，
# 无真实微信 / Redis / Postgres。
add_executable(notify_service_test
    tests/test_notify_service.cpp
    src/services/src/NotifyService.cpp
    src/utils/src/Cooldown.cpp
    src/db/src/RedisClient.cpp
    src/db/src/PendingEventRepository.cpp
)
target_include_directories(notify_service_test PRIVATE ${GATEWAY_INCLUDE_DIRS})
target_link_libraries(notify_service_test PRIVATE
    Drogon::Drogon nlohmann_json::nlohmann_json ${HIREDIS_LIBRARY})
add_test(NAME notify_service_test COMMAND notify_service_test)

# 单条告警详情单测 —— 内存仓库，无真实 Postgres。
add_executable(alert_service_test
    tests/test_alert_service.cpp
    src/services/src/AlertService.cpp
)
target_include_directories(alert_service_test PRIVATE ${GATEWAY_INCLUDE_DIRS})
target_link_libraries(alert_service_test PRIVATE
    Drogon::Drogon nlohmann_json::nlohmann_json)
add_test(NAME alert_service_test COMMAND alert_service_test)

# 模板 ID 权威来源单测 —— 纯内存，无真实依赖。
add_executable(subscription_service_test
    tests/test_subscription_service.cpp
    src/services/src/SubscriptionService.cpp
)
target_include_directories(subscription_service_test PRIVATE ${GATEWAY_INCLUDE_DIRS})
target_link_libraries(subscription_service_test PRIVATE
    Drogon::Drogon nlohmann_json::nlohmann_json)
add_test(NAME subscription_service_test COMMAND subscription_service_test)

# user_roles 实时角色单测 —— roleOf 优先级归一（admin > teacher），纯内存，
# 无真实依赖。
add_executable(user_role_test
    tests/test_user_role.cpp
)
target_include_directories(user_role_test PRIVATE ${GATEWAY_INCLUDE_DIRS})
add_test(NAME user_role_test COMMAND user_role_test)

# Postgres-backed e2e test — built only when libpq is available. Skips at
# runtime unless PG_TEST_CONNINFO is set.
if (PostgreSQL_FOUND)
    add_executable(pg_retry_test
        tests/test_pg_retry.cpp
        src/services/src/RetryWorker.cpp
        src/db/src/PendingEventRepository.cpp
        src/db/src/PgPool.cpp
        src/db/src/PgPendingEventRepository.cpp
    )
    target_include_directories(pg_retry_test PRIVATE
        ${GATEWAY_INCLUDE_DIRS} ${PostgreSQL_INCLUDE_DIRS})
    target_link_libraries(pg_retry_test PRIVATE
        Drogon::Drogon nlohmann_json::nlohmann_json ${PostgreSQL_LIBRARIES})
    add_test(NAME pg_retry_test COMMAND pg_retry_test)

    add_executable(pg_repos_test
        tests/test_pg_repos.cpp
        src/db/src/PgPool.cpp
        src/db/src/PgSpaceRepository.cpp
        src/db/src/PgAlertRepository.cpp
        src/db/src/PgNotifyTargetRepository.cpp
        src/db/src/PgUserRoleRepository.cpp
        src/db/src/PgUserSpacesRepository.cpp
        src/db/src/PgWechatBindingRepository.cpp
    )
    target_include_directories(pg_repos_test PRIVATE
        ${GATEWAY_INCLUDE_DIRS} ${PostgreSQL_INCLUDE_DIRS})
    target_link_libraries(pg_repos_test PRIVATE
        nlohmann_json::nlohmann_json ${PostgreSQL_LIBRARIES})
    add_test(NAME pg_repos_test COMMAND pg_repos_test)
endif()
