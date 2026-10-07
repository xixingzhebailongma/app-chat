# config/config.json 字段说明

> **网关运行时只读 `config/config.json`**（`src/main.cpp` 里的
> `loadConfigFile("config/config.json")`）。本目录**不再有 `config.yaml`**（已删除，
> 避免「改错文件」的双源问题）。改配置一律改 `config.json`；生产秘密一律经环境
> 变量 / K8s Secret 注入，**不写进仓库**（见 `deploy/k8s.yaml`）。

本文件是把原 `config.yaml` 里的注释说明迁移过来的字段参考，字段名与 `config.json`
逐项对齐，避免幽灵配置。改 `config.json` 时以本文为准理解各字段含义。

## 另外三份适配配置（7.9 改造，配置驱动壳）

- `config/error_codes.json` —— 网关错误码（字符串）↔ HTTP 状态
- `config/error_envelope.json` —— 错误信封形状（默认嵌套，flat 可选）
- `config/upstream.json` —— 上游 go-backend 路径/字段名

三者默认值等价改造前现状；改动方法见 `docs/改造指南.md`，默认假设见 `docs/默认假设.md`。

## 顶层结构

所有字段在 `custom_config` 之下。字段一览：

`listen_port` · `internal_token` · `jwt` · `wechat` · `go_backend` · `redis` ·
`postgres` · `operation_log` · `notify`

## 各字段说明

### listen_port
- 监听端口，默认 `8080`。

### internal_token
- `/internal/*` 内部令牌（`InternalTokenFilter` 校验 `X-Internal-Token` 头）。
- dev 默认 `dev-internal-token`；prod 经环境变量 `INTERNAL_TOKEN` 注入。

### jwt
- `secret`：与 go-backend 共享的 JWT 密钥；网关只签名/校验，不另起一套 token。prod 经 `JWT_SECRET` 注入。
- `access_ttl_seconds`：访问令牌有效期，默认 `259200`（72h，与 go-backend 一致）。
- `openid_ttl_seconds`：OA 绑定 `openid_token` 的短期 TTL，默认 `300`（5 分钟）。

### wechat
- `api_base_url`：微信 API 基址，默认 `https://api.weixin.qq.com`；本地验收可指向 mock。经 `WECHAT_API_BASE_URL` 覆盖。
- `miniapp.appid` / `miniapp.secret`：小程序凭证（`jscode2session` + 订阅消息 access_token）。prod 经 `WECHAT_APPID` / `WECHAT_SECRET` 注入。
- `oa.appid` / `oa.secret`：公众号（服务号）凭证，用于 access_token 缓存。prod 经 `WECHAT_OA_APPID` / `WECHAT_OA_SECRET` 注入。

> **prod 快速失败**：`APP_ENV=prod` 时，若 `internal_token` / `jwt.secret` /
> `wechat.miniapp.secret` / `wechat.oa.secret` 任一为空或仍是 dev 占位符
> （`dev-*` / `CHANGE_ME`），网关拒绝启动（`LOG_FATAL`）。

### go_backend
- `base_url`：go-backend 基址，默认 `http://127.0.0.1:8081`。经 `GO_BACKEND_URL` 覆盖。
- `api_prefix`：网关转发到边侧时补的前缀，默认 `/api`。
- 拼接规则：`base_url + api_prefix + 路径`（`GoBackendClient`）。

### redis
- `host` / `port` / `password`：支撑 `wechat:token:{miniapp,oa}`、刷新锁、通知冷却。
- host 为空 → 进程内回退（开发）。prod 经 `REDIS_HOST` / `REDIS_PORT` / `REDIS_PASSWORD` 注入。

### postgres
- `conninfo`：libpq 连接串。空 → 内存仓库回退（开发/测试）。
- 环境覆盖 `PG_CONNINFO`；prod 为空则拒绝启动。

### operation_log
- `file`：运维操作日志 JSONL 落盘文件，默认 `operation_logs.jsonl`；空串禁用文件兜底。

### notify —— 通知渠道（7.9 插件化）

**渠道通用开关语义**：`channels.<name>.enabled` 决定该渠道是否作为主渠道候选参与
分发（`NotifyService` 发送时过滤）；`mode: "mock" | "real"` 决定是否真发。

| 渠道 | 说明 | 真实凭证注入 |
|---|---|---|
| `wechat_miniapp` | 订阅消息（教师/管理员主渠道） | 凭证走 `wechat.miniapp`；模板 ID 经 `MINIAPP_TMPL_*` 覆盖 |
| `sms` | 短信兜底 | 经 `SMS_ACCESS_KEY` / `SMS_SECRET` 注入 |
| `dingtalk` | 钉钉工作通知 | `DINGTALK_APP_SECRET`（app_key / agent_id 走 config） |
| `wecom` | 企业微信应用消息 | `WECOM_CORP_SECRET`（corp_id / agent_id 走 config） |

- `wechat_miniapp.templates`：`event_type → {id, type, fields}`。4 个事件
  `device_offline` / `peripheral_offline`（共用「设备离线」模板）/ `sensor_threshold` /
  `face_login_failed`。`type` 为 `one_time`（默认）/ `long_term`（需教育类目资质）；
  模板数 ≤3（微信单次 `requestSubscribeMessage` 上限）。真实模板 ID 经
  `MINIAPP_TMPL_DEVICE_OFFLINE` / `MINIAPP_TMPL_PERIPHERAL_OFFLINE` /
  `MINIAPP_TMPL_SENSOR_THRESHOLD` / `MINIAPP_TMPL_FACE_LOGIN_FAILED` 注入，不硬编码。
  `mode=real` + `APP_ENV=prod` 时仍为 `tmpl_*` 占位 → 拒绝启动。
- `sms.templates`：`event_type → {code, params}`（短信模板 CODE，需短信服务商审核）。
  `provider`：`aliyun` / `tencent`；`sign_name` 签名；`region`；`sdk_app_id`。
- `routing.default`：事件 → 主渠道列表（未命中事件时的兜底路由）。
- `cooldown.default_seconds`：冷却窗口，默认 `1800`（30 分钟，按
  event_type+device_id+订阅者+渠道去重）。
- `fallback.enabled` / `fallback.channel`：主渠道失败后短信兜底（默认 `sms`）。
- `oa.arrival_template_id`：家长到校模板 ID（经 `WECHAT_OA_TMPL_ARRIVAL` 覆盖）；
  `oa.notify_once_per_day`：同 (student_no, space_id) 当日只推一次；
  `oa.notify_on_leave`：离校通知默认关闭；`oa.sms`：绑定短信验证码（`mode=real` 才真发）。
- `retry`：重试策略（`max_attempts` / `base_delay_seconds` / `max_delay_seconds` /
  `jitter_ratio` / `poll_interval_seconds` / `batch_size`）。

## mock → real 切换速查

1. 把对应渠道的 `mode` 由 `"mock"` 改为 `"real"`。
2. 注入真实凭证（上表「真实凭证注入」列）与审核通过的真实模板 ID。
3. `APP_ENV=prod` 且 `mode=real` 时，占位 `tmpl_*` / 空模板 ID 会快速失败——先补真实值再上线。

完整切换步骤见 `docs/上线切真实模式-runbook.md`；短信专项见 `docs/短信真实模式.md`；
小程序订阅消息专项见 `docs/小程序订阅消息真实模式.md`。
