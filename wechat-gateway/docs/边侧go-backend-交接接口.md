# 边侧 go-backend 交接接口文档

> 说明：`wechat-gateway`（C++ Drogon）是网关/BFF，边侧 `go-backend` 是**独立外部服务**（非本仓库）。
> 两者之间有两个方向的契约：
> - **A 网关 → 边侧**：边侧**需要实现**、供网关调用的接口。
> - **B 边侧 → 网关**：边侧/Agent**主动回调**网关的内部接口（`/internal/*`）。
>
> 权威契约：`config/upstream.json`（A 面路径/字段映射）、`openapi/internal.yaml`（B 面）、`src/clients/GoBackendClient.cpp`（A 面调用实现）、`src/controllers/WsProxyController.cpp`（WS 代理）。

## 修订记录（修改点标注）

| 点 | 内容 | 状态 |
|---|---|---|
| 1 | Token 体系（设备/控制透传前端 JWT，非边侧 token） | 已更正 |
| 2 | api_prefix 与路径拼接口径统一 | 已更正 |
| 3 | user_spaces → 边侧 face 底库同步 | 缺口，标注待补 |
| 4 | WS 路由与连接管理专节 | 已补 |
| 5 | 全量同步防误删（dry_run/阈值/软删/明细） | 现状说明 + 待实现 |
| 6 | 告警落库闭环 + 状态枚举 + handle 契约 | 已补（状态为 unhandled/handling/resolved） |
| 7 | 场景匹配能力字段（zb_role/capabilities） | 现状说明 + 建议 |
| 8 | wechat-gateway 与「云端 Go 后端」关系 | 已澄清（见第 0 节） |

## 已知缺口清单（待排期，一行一个）

| # | 缺口 | 位置 | 影响 |
|---|---|---|---|
| G1 | `user_spaces` 未同步到边侧 face 底库 | C.2 | 小程序绑定教室后，综合屏刷脸拉不到该教师底库 |
| G2 | 全量同步无 `dry_run` / 阈值保护 / 软删 / 明细日志 | B.6 / B.7 | 传部分列表会真实删掉 edge 空间及其绑定/门禁标记 |
| G3 | 设备能力字段 `zb_role` / `capabilities` 缺失 | A.7 | 场景匹配只能靠 label 关键字，存在误/漏匹配风险 |

---

## 0. 术语与架构澄清【修改点 8】

- **wechat-gateway 就是「云端后端」（BFF）**，C++ Drogon，本仓库。
- 旧设计文档（含 CLAUDE.md 第七章）里的「**云端 Go 后端**」是**误称**，指的就是本仓库的 wechat-gateway。**不存在第二个云端 Go 后端**，二者是「**同一件事的两个叫法**」，不是替代、也不是并存。
- 整个体系里**唯一的 Go 服务是边侧 go-backend**（仓库外、别人负责），它是「边侧」，不是「云端」。
- 因此：权威用户/设备数据在**边侧 go-backend**，网关（wechat-gateway）做**代理 + 缓存 + 鉴权 + 推送**；user_spaces / door_devices / 告警处理 / 运维日志的权威在**网关 PG**（见第 C 节）。

---

## A. 网关 → 边侧（边侧需要实现的接口）

### A.1 连接与 Token 体系【修改点 1、2】

**路径拼接（统一口径，避免 `/api/api/...`）**

- `最终 URL = base_url + api_prefix + path`
- `base_url`：`config.json → go_backend.base_url`（默认 `http://127.0.0.1:8081`，环境变量 `GO_BACKEND_URL` 覆盖）
- `api_prefix`：默认 `/api`（无环境变量覆盖，只能改 config）
- `path`：来自 `config/upstream.json`，**不含 `/api` 前缀**。
- **下文 A.2~A.6 列出的路径均为「最终 URL 路径」，已含 `api_prefix`。**

**Token 体系（三把 token，用途不同，务必分清）**

| 场景 | 网关发送的头 | 说明 |
|---|---|---|
| 登录校验（A.2） | 无鉴权头，body 传 username/password | 边侧返回**边侧 token**；网关**不缓存、不刷新**，仅用于绑定瞬间校验账号、取回 user_id/role/name，随即丢弃并自签前端 JWT |
| 设备列表（A.3）/ 设备控制（A.4） | `Authorization: Bearer <前端 JWT>` | **透传调用方原始前端 JWT**（网关 login/bind 签发的），**不是边侧 token，也不是 X-Internal-Token** |
| 手机号查询（A.5） | `X-Internal-Token: <网关内部令牌>` | 短信兜底解析手机号用 |
| WS（A.6） | 透传 `token` + `space_id` 两个 query 参数 | 见 A.6 |

- **前端 JWT**：HS256，claims `{user_id, role, exp}`，密钥 `config.jwt.secret`（默认 `dev-shared-jwt-secret`，环境变量 `JWT_SECRET`），由网关签发。
- **边侧设备接口鉴权（二选一）**：
  1. **校验前端 JWT**：与网关共享同一 HS256 密钥（`JWT_SECRET` 两边同值），边侧自行验签后按 `user_id/role` 做授权；
  2. **信任网关**：网关已完成空间边界校验（`canAccessSpace`）+ 门禁二次确认（`confirm`），边侧收到请求只执行、不再鉴权。
- **密钥轮换**：`JWT_SECRET` 是网关↔边侧共享配置，轮换时两边必须同步切换。建议经配置中心统一下发，并用「双密钥过渡」（新旧各验一段窗口）避免滚动期间拒请求。
- **边侧 token**：只在 bind 流程短暂存在（A.2 返回），网关不落库、不续期，**不要依赖它做设备接口鉴权**。

### A.2 `POST /api/auth/login` —— 账号校验（`GoBackendClient::login`）

网关在 `POST /api/miniapp/bind` 时调用，校验账号并取回用户身份。

- 请求 body：`{"username":"...","password":"..."}`
- 成功响应（200）：
  ```json
  { "token": "<边侧会话令牌>", "user": { "user_id": "...", "username": "...", "name": "...", "role": "admin|teacher" } }
  ```
- 约束：`user.user_id` 可为 int 或 string（网关统一规范化为十进制字符串）；`role` 缺省按 `teacher`；账号错误回 `401` + `{"message":"..."}`。

### A.3 `GET /api/devices?space_id=<sid>` —— 设备列表（`DeviceService::listDevices`）

- 请求头：`Authorization: Bearer <前端 JWT>`（网关已做空间边界校验后透传）。
- 响应：设备对象**数组**（网关原样包装为 `{"devices":[...]}`）。单设备字段：

  | 字段 | 说明 |
  |---|---|
  | `device_id` | 设备唯一 ID |
  | `device_type` | `zigbee`（灯/门禁/传感器）/ `fuhe-screen`（综合屏）/ `fuhe-board`（班牌） |
  | `status` | `online` / `offline` |
  | `app` | 开关态 `on/off`；传感器为 JSON 字符串（温度/湿度/CO₂） |
  | `label` | 显示名（场景匹配兜底用，见 A.7） |
  | `space_id` | 空间 ID（与网关 `user_spaces.space_id` **同源同值**） |
  | `last_seen` | ISO 时间 |
  | 可选 | `gateway_id`、`zb_type`、`zb_role`（见 A.7）、`info` |

- 边侧不可达时网关回 `503`（`GO_BACKEND_UNAVAILABLE`）。

### A.4 `POST /api/devices/{device_type}/{device_id}/cmd` —— 设备控制（`DeviceControlService::control`）

- 请求头：`Authorization: Bearer <前端 JWT>`。
- 请求 body：`{"command":"on|off|toggle"}`。
- 响应：**原样透传**（网关不解析，直接回给前端）；边侧执行后经 WS 回推 `device_update` / `cmd_response`。
- `{device_type}` 取设备列表的 `device_type`；`{device_id}` 取 `device_id`。
- 门禁语义（网关侧已拦）：门禁白名单内 `off` 需前端带 `confirm=true`（否则网关直接 428 不下发）、`toggle` 网关直接 400 不下发、`on` 直接下发。边侧收到 `command` 直接执行即可，不需要再判断二次确认。

### A.5 `GET /api/users/{user_id}` —— 手机号查询（`GoBackendClient::getUserPhoneSync`）

- 请求头：`X-Internal-Token: <网关内部令牌>`。
- 响应：`{"user_id":"...","phone":"138..."}`；用户不存在 `404`。

### A.6 `WS /ws` —— 设备实时事件源 + 连接管理【修改点 4】

网关小程序侧 `/ws` 连入后，网关作为客户端连边侧 `/ws` 并透传。

**连接模型（1:1 代理，不聚合）**

- 每个小程序 WS 连接 → 网关**独立开一条**边侧 WS（`drogon::WebSocketClient` 每连接一个）。
- 网关把 `token` + `space_id` 作为 query 参数透传给边侧 `/ws`；边侧按 `space_id` 订阅（空串 = 全部空间）。
- 每条连接只订**一个 `space_id`**。管理员用空串订全部；**教师绑定多个教室时，前端一次只连一个空间**（需要多个空间 = 前端发起多条连接，或切换空间时重连）。

**消息透传与路由**

- 边侧 → 小程序：`device_update` / `cmd_response` / `ping` **原样逐条转发**，网关**不按 device_id 路由、不聚合、不过滤**；`cmd_response` 是边侧按空间广播，网关原样回给该连接，**由小程序前端自己按 device_id 匹配**（对应 `devices.vue` 的 onMessage）。
- 小程序 → 边侧：原样透传。

**断线与重连**

| 事件 | 网关行为 | 重连责任 |
|---|---|---|
| 边侧 WS 断开 | 网关**主动关闭**对应的小程序连接（`forceClose`），**不自动重连边侧** | 由小程序侧重新发起 `/ws` 连接 |
| 小程序连接断开 | 网关**停止**对应的边侧客户端 | 前端 `onShow`/`onClose` 重新 `connectWs` |

**结论**：网关是纯透传代理，不缓存连接、不重连、不按 device_id 分流；重连与 device_id 匹配都在前端。

### A.7 设备能力字段与场景匹配【修改点 7】

**现状（真实边侧无 `zb_role`）**

- 场景匹配（`SceneDefinitions.h`）：`device_type` **精确匹配**（唯一必填）+ `label` 关键字兜底（`label_include` 命中 / `label_exclude` 排除，大小写不敏感）。
- `zb_role` 在规则里是**保留字段**，真实边侧设备列表无 `zb_role` 时留空、纯靠 label。
- 风险：label 是管理员可改的，关键字匹配存在误匹配/漏匹配风险（当前用关键字列表 + `label_exclude` 缓解）。

**建议（待边侧补字段）**

- 边侧设备对象**新增能力字段**（`zb_role` 或 `capabilities`，如 `light` / `door` / `screen` / `sign` / `sensor`）。
- 场景匹配**优先用能力字段**，`label` 关键字仅做兜底。边侧补字段后，`SceneDefinitions.h` 的 `zb_role` 即可启用优先级匹配。

---

## B. 边侧 → 网关（边侧/Agent 调用的内部接口）

- 鉴权：请求头 `X-Internal-Token`（`config.internal_token`，环境变量 `INTERNAL_TOKEN` 覆盖；dev 默认 `dev-internal-token`）。错误令牌 → `401`。
- **全部 `POST`，全部走 `/internal/*`，不对小程序/前端开放。**

### B.1 `POST /internal/notify/send` —— 告警/事件推送入口 + 落库闭环【修改点 6】

- body（`event_type`、`content` 必填）：
  ```json
  {
    "event_type": "device_offline | sensor_threshold | face_login_failed | peripheral_offline",
    "content": "综合屏离线",
    "event_id": "alt_xxx",            // 有则落库（alert_id=event_id），缺省只推送不落库
    "space_id": "spc_...",
    "device_id": "dev_...",
    "device_label": "A101 综合屏",
    "severity": "warning",
    "transition": "online->offline",  // 防御性复核用
    "duration_sec": 300,              // 缺省/哨兵值(-1)不抑制
    "consecutive": 3,
    "offline_minutes": 30
  }
  ```
- **落库约定**：凡「需要管理员在小程序里查看历史 / 标记处理 / 写备注」的告警，**必须带 `event_id`** 才会落库；缺 `event_id` 只推送、不落库（之后无法处理）。
- 语义：网关负责「推给谁、怎么推、冷却」；**触发条件判断（在线变离线/持续5分钟/连续≥3次/离线30分钟）在边侧**，网关只做防御性复核（字段缺失时不抑制）。
- 事件类型 4 类，未知类型跳过；冷却 1800s（键 `event_type+device_id+user+channel`）。

**告警状态与处理闭环（小程序侧，非边侧）**

- 告警状态枚举（**三态**）：`unhandled`（未处理）→ `handling`（处理中）→ `resolved`（已解决）。**不是 pending/processing/resolved**。
- 处理接口（管理员）：`POST /api/miniapp/alerts/{id}/handle`，body `{"status":"handling|resolved","remark":"..."}`，响应 200；会写运维日志（`operation_logs`），详情见《前端 miniapp 交接接口文档》3.5 节。

### B.2 `POST /internal/notify/subscribe-send` —— 小程序订阅消息单发

- body：`{openid, template_id, data:{"thing1":"v1",...}, page?, miniprogram_state?}`（前三者必填）。
- 响应：`{success, channel, errcode, errmsg}`。

### B.3 `POST /internal/oa/arrival-notify` —— 家长到校通知

- body：`{student_no, student_name, space_id, space_name, arrival_time, event_id?, event_type? arrival|leave}`。
- 响应：`{accepted, notify_id, final_status: no_parent_binding|skipped|success|failed|duplicate, channel: wechat_oa|none, reason}`。

### B.4 `POST /internal/user-roles/sync` —— 角色全量同步（幂等）

- body：`{user_id, roles:["teacher"]}`；`roles` 空数组 = 清空该用户全部角色。
- 用途：go-backend 权威角色回灌网关 `user_roles`（只读缓存）；网关 `JwtFilter` 每请求实时覆盖 JWT 内嵌 role（60s 缓存），**同步后无需重新登录**。

### B.5 `POST /internal/access-records` —— 进出记录回灌（`event_id` 幂等）

- body：`{event_id, space_id, occurred_at, user_id?, name?, auth_type?, result?, device_id?}`（前三者必填）。

### B.6 `POST /internal/spaces/sync` —— 空间生命周期全量同步【修改点 5】

- body：`{spaces:[{space_id, name, type}, ...]}`（**必须全量**）。
- 响应：`{ok, upserted, removed}`（**仅返回计数**）。

**当前删除规则（比「未列出即删」更精确）**

- 只删「`source=edge` 且不在本次列表里」的空间；**`source=admin` 的空间永不删**（网关管理员建的教室不被边侧覆盖）。
- 被删空间会**级联清理** `user_spaces` / `door_devices`。
- **现状缺口**：无 `dry_run`、无删除比例阈值保护、无软删除/恢复窗口、无逐条 upserted/removed 明细日志。**传部分列表会真实删掉 edge 空间及其绑定/门禁标记，务必传全量。**

**建议（待实现）**

1. 加 `dry_run` 参数，先返回「将删除的空间列表」。
2. 加删除阈值保护（如本次 `removed` 超过现存 edge 空间的一定比例则拒绝）。
3. 软删除 + 恢复窗口（替代硬删）。
4. 记录本次 upserted / removed 的逐条明细（写入运维日志）。

### B.7 `POST /internal/student-parents/sync` —— 学生花名册全量同步【修改点 5】

- body：`{parents:[{student_no, parent_phone, student_name, relation, status: active|inactive}, ...]}`（**必须全量**）。
- 响应：`{ok, upserted, removed}`。
- 同 B.6 的现状缺口：无 `dry_run`/阈值保护/明细日志，建议一并补齐。

---

## C. 数据权威性划分 + user_spaces 同步【修改点 3】

### C.1 谁拥有什么

| 数据 | 权威方 | 网关角色 |
|---|---|---|
| 用户账号/密码校验 | go-backend | 代理（bind 走边侧 login） |
| 用户角色（admin/teacher） | go-backend | `user_roles` 只读缓存（经 `/internal/user-roles/sync` 全量回灌） |
| 设备 / 设备状态 | go-backend | 代理 + 缓存（`/api/devices`、`/ws`） |
| 教师↔教室绑定（user_spaces） | **网关 PG** | `SpaceBindingService` / `/api/miniapp/space-bindings`，Go 不实现 |
| 门禁白名单（door_devices） | **网关 PG** | 设备页「标记门禁」，Go 不实现 |
| 告警处理状态 / 运维日志 | **网关 PG** | `/api/miniapp/alerts/{id}/handle` + operation-logs，Go 不实现 |
| 空间（space_id 权威） | 边侧回灌 + 网关 admin 增删改 | `/internal/spaces/sync`（仅管 edge 空间）+ `/api/admin/spaces`（admin 空间） |
| 花名册（家长绑定源） | 边侧/学校 | `/internal/student-parents/sync` 全量回灌 |

### C.2 user_spaces → 边侧 face 底库【缺口，待补】

- **现状**：网关 PG 的 `user_spaces`（教师↔教室绑定）**不会同步到边侧**；网关侧没有向边侧推送绑定关系的接口。
- **影响**：若边侧综合屏人脸识别需要「`GET /api/face/gallery?space_id=` 返回本空间已授权教师底库」，那么**小程序里绑定了教室，边侧底库拉不到该教师**——两端不一致。
- **待补**（需与边侧约定）：
  1. 同步**方向**：网关主动推给边侧（新增 `/internal/*` 推送）或边侧定时拉取（新增网关查询接口）。
  2. 同步**频率**：实时/准实时（绑定变更即触发）或定时对账。
  3. **冲突裁决**：`user_spaces` 权威在网关 PG，边侧底库以网关为准；边侧不可反向改。
  4. 删除同样要全量/增量语义清晰，避免残留。

**关键约束（不变）**：所有设备/告警/人脸/刷卡事件必须带 `space_id`，且与 `user_spaces.space_id` **同源同值**；教师「仅本人绑定教室」即网关拿 `user_spaces.space_id` 匹配这些 `space_id`，不同源会导致教师端全空。
