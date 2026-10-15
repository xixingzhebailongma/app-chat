# 微信小程序前端（miniapp）交接接口文档

> 服务：**wechat-gateway**（C++ Drogon，本仓库），监听 `0.0.0.0:8080`。
> 权威机器可读契约：`wechat-gateway/openapi/miniapp.yaml`（对外硬契约）。本文为面向前端开发的人肉交接版，字段以 yaml + 源码为准。

> **前端范围**：本仓库有两套前端——**miniapp**（教师/管理员小程序，本文档）与 **parent-h5**（家长绑定 H5，走 `/api/oa/bind/*`），二者接口不同、互不相通；**综合屏（`fuhe-screen`）是小程序管理的设备类型，不是独立前端**。

## 1. 连接与鉴权

| 项 | 值 |
|---|---|
| 环境 | 本地 dev：`http://127.0.0.1:8080`；生产走部署域名（HTTPS） |
| 鉴权 | 登录/绑定后拿 JWT，请求头 `Authorization: Bearer <JWT>` |
| JWT | HS256，claims `{user_id, role, exp}`；网关签发，前端只透传不解析。**role 一律以 `/api/miniapp/me` 为准**（`JwtFilter` 每请求实时查 `user_roles` 覆盖，60s 缓存；仅 `user_roles` 无记录时回退 JWT 内嵌 role） |
| 角色字面量 | 仅 `admin` / `teacher`（多角色按 admin > teacher 归一） |
| 统一错误体 | `{"error":{"code":"<稳定码>","message":"..."}}` |
| 空间边界 | 教师「仅本人绑定教室」由网关按 `user_spaces` + 请求 `space_id` 过滤 |

**接口分三类**：
- **公开**（无鉴权）：`/health`、`/api/miniapp/login`、`/api/miniapp/bind`、`/api/oa/bind/*`。
- **鉴权**（Bearer JWT）：其余 `/api/*`。
- **内部**（`/internal/*`，`X-Internal-Token`）：**不对前端开放**，仅供边侧 Agent / go-backend 回调。

> 约定：所有设备/告警/人脸/刷卡事件都带 `space_id`，且与 `user_spaces.space_id` **同源同值**；不同源会导致教师端数据全空。

### 1.1 环境切换 / baseURL（Vite env）

前端通过 `.env` / `.env.local` / `.env.example` 注入（`import.meta.env`）：

| 变量 | 默认 | 说明 |
|---|---|---|
| `VITE_BASE_URL` | `http://127.0.0.1:8080` | 网关 REST baseURL |
| `VITE_WS_BASE_URL` | 空 | WebSocket baseURL；空则 http→ws 推导 |
| `VITE_SUBSCRIBE_TMPL_IDS` | 空 | 订阅消息模板 ID（逗号分隔） |
| `VITE_USE_MOCK` | `true` | **接真实网关必须显式置 `false`**，否则所有请求打到本地 mock 而非网关 |

dev 走 Vite 代理：`/api`、`/internal`、`/ws` 均代理到 `http://127.0.0.1:8080`。

## 2. 最小可用流程（happy path）

wx.login → `POST /api/miniapp/login`（未绑定得 `{need_bind:true, openid_token}`）
→ `POST /api/miniapp/bind`（得 `{token, user_id, role, name}`）
→ `GET /api/miniapp/spaces` → `GET /api/miniapp/devices?space_id=`
→ 连 `/ws`（token + space_id）→ `POST /api/miniapp/device/control`
→ 监听 `device_update` 刷新 UI
→ 任何 `401`：清 token → 重走 `wx.login` → `login`/`bind`。

---

## 3. 公开接口

### 3.1 `GET /health`
存活探测，恒 200。响应：`{"status":"ok","service":"wechat-gateway"}`（及 db/redis/channels 状态）。

### 3.2 `POST /api/miniapp/login` —— 微信登录
- body：`{code: "<wx.login 的 code>"}`
- 响应（已绑定）：`{token, user_id, role, name}`
- 响应（未绑定）：`{need_bind: true, openid_token}`，前端拿 `openid_token` 走 bind

### 3.3 `POST /api/miniapp/bind` —— 账号绑定（真实登录链）
- body：`{openid_token, username, password}`
- 流程：网关拿 `openid_token` 解析 openid → 调边侧 `login` 校验账号 → 落 `wechat_bindings` → 签发 JWT
- 响应：`{token, user_id, role, name}`

### 3.4 家长公众号绑定（`/api/oa/bind/*`，前端小程序一般不用）
`GET /api/oa/bind/authorize?code=`、`POST /api/oa/bind/send-code`、`POST /api/oa/bind/confirm`、`GET /api/oa/bind/me?openid_token=`、`POST /api/oa/bind/unbind`。详见 `openapi/miniapp.yaml`。

---

## 4. 鉴权后接口（按模块）

通用分页信封：`{..., "total":N, "page":p, "page_size":s, "has_more":bool}`。

- **默认值/上限**：`page` 默认 `1`（上限 `10000`）、`page_size` 默认 `50`（上限 `200`）；`has_more = page*page_size < total`。
- **`from` / `to` 是时间范围过滤，不是分页**：作用于 `created_at`，格式 ISO-8601 UTC（如 `2026-10-15T00:00:00Z`）。

### 4.1 身份
| 方法 | 路径 | 说明 | 响应 |
|---|---|---|---|
| GET | `/api/miniapp/me` | 回显身份 | `{user_id, role, name}`（未绑定时 `name` 为空串） |

### 4.2 空间 / 空间类型
| 方法 | 路径 | 角色 | 说明 / body / 响应 |
|---|---|---|---|
| GET | `/api/miniapp/spaces` | 登录即可 | admin 全部 / teacher 仅绑定空间；`{spaces:[{space_id,name,type}]}` |
| GET | `/api/admin/spaces` | admin | 含停用 + `source`；`{spaces:[{space_id,name,type,enabled,source}]}`，`source ∈ edge | admin`（`edge`=边侧同步，`admin`=管理员新建） |
| POST | `/api/admin/spaces` | admin | `{name, type, space_id?}`（type 必须在 space_types；space_id 缺省自动生成） |
| PUT | `/api/admin/spaces/{space_id}` | admin | `{name?, type?, enabled?}` |
| DELETE | `/api/admin/spaces/{space_id}` | admin | 软删 `enabled=false` |
| GET | `/api/miniapp/space-types` | 登录即可 | `{types:[{code,name,sort_order,enabled,icon}]}`（含停用，按 sort_order） |
| POST | `/api/admin/space-types` | admin | `{code, name, icon?, sort_order?}` |
| PUT | `/api/admin/space-types/{code}` | admin | `{name?, icon?, enabled?}` |
| DELETE | `/api/admin/space-types/{code}` | admin | 软删 `enabled=false` |
| PATCH | `/api/admin/space-types/order` | admin | `{codes:[...]}` 按下标写 sort_order |

### 4.3 设备
| 方法 | 路径 | 角色 | 说明 |
|---|---|---|---|
| GET | `/api/miniapp/devices?space_id=` | admin+teacher | **转发边侧**，空间边界；响应 `{devices:[{device_id, device_type, status, app, label, ...}]}` |
| POST | `/api/miniapp/device/control` | admin+teacher | **转发边侧**；body `{space_id, device_type, device_id, command, confirm?}`，`command ∈ on/off/toggle`，`confirm` 为 JSON **boolean**（默认 `false`） |

- **异步语义**：`POST` 返回 200 仅表示指令已下发/转发边侧成功，**不等于设备已执行**；真实执行结果靠 WS `device_update`（权威）回填，`cmd_response` 仅作回执提示。**前端必须监听 WS 更新 UI，不能把 POST 返回值当作执行结果。**
- 门禁（`door_devices` 白名单内）**只显示 `on` / `off`，不提供 toggle 按钮**：`off` 需二次确认弹窗、确认后带 `confirm=true` 重发（否则 `428 CONFIRM_REQUIRED`）；`toggle` 直接 `400`；`on` 不确认。
- **`on` = 锁门、`off` = 开门（开锁）** —— 语义反直觉，前端极易搞反。
- `device_type` 取设备列表返回的 `device_type`（灯/门禁=`zigbee`、综合屏=`fuhe-screen`、班牌=`fuhe-board`、传感器=`zigbee`）。

### 4.4 场景
| 方法 | 路径 | 说明 |
|---|---|---|
| GET | `/api/miniapp/scenes?space_id=` | 懒种「开启模式/离开模式」，响应 `{scenes:[{scene_id,space_id,name,kind,device_states}], active_scene_id}` |
| POST | `/api/miniapp/scenes` | `{space_id, name, device_states:[{device_id,device_type,command}]}`（`command ∈ on/off`，强制 kind=custom），响应 `{ok, scene_id}` |
| PUT | `/api/miniapp/scenes/{scene_id}` | `{name?, device_states?}`（kind 不可改） |
| DELETE | `/api/miniapp/scenes/{scene_id}` | 硬删；若为激活场景一并清空 |
| POST | `/api/miniapp/scenes/{scene_id}/execute` | **无 body**；内置 `all_on/all_off` 按 `SceneDefinitions.h` 规则枚举下发，custom 按 device_states 精确下发；响应 `{ok, applied, results, warning?}` |

- execute 响应字段：`applied` = 已下发目标数（`results` 长度，**非成功数**）；`results[]` = `{device_id, command, status}`（`status` 为每设备下发 HTTP 状态，传输失败为 `0`）；`warning` = `""` / `"未匹配到可控制设备"` / `"设备命令下发失败"`。custom 全部失败仍 200 + warning。

内置场景含义：`all_on`=开灯+唤醒综合屏+开班牌；`all_off`=关灯+关屏+关班牌（**不碰门锁**）。

### 4.5 告警
| 方法 | 路径 | 角色 | 说明 |
|---|---|---|---|
| GET | `/api/miniapp/alerts` | 登录 | query `space_id,event_type,status,from,to,page,page_size`；响应 `{alerts:[{alert_id,space_id,event_type,status,operator_id,operator_name,remark,device_id,device_label,created_at}], total,page,page_size,has_more}` |
| GET | `/api/miniapp/alerts/{id}` | 登录 | **教师越权与不存在统一 404**（隐藏存在性）；其余无权限 403 |
| GET | `/api/miniapp/alerts/{id}/timeline` | admin | `{alert:{...}, timeline:[...]}` |
| POST | `/api/miniapp/alerts/{id}/handle` | admin | `{status: handling|resolved, remark?}`；缺省 `resolved`；**不能设回 `unhandled`**；写运维日志 |
| GET | `/api/miniapp/alerts/stats` | 登录 | 见下方结构示例 |

- 状态三态：`unhandled`（入库默认/边侧 upsert）→ `handling` → `resolved`；`handle` 仅接受 `handling | resolved`，**不能设回 `unhandled`**。
- **脱敏**：teacher 视角 `operator_id/operator_name/remark` 恒为空串。
- stats 结构示例：
  `{"by_status":{"unhandled":0,"handling":0,"resolved":0},"by_event_type":{...},"by_space":[{"space_id","count"}],"by_space_unhandled":[{"space_id","count"}]}`
  （`by_status` 三键恒在、缺省补 0；`by_space*` 仅 admin。）

### 4.6 订阅消息
| 方法 | 路径 | 说明 |
|---|---|---|
| GET | `/api/miniapp/subscribe` | 额度回显 `{user_id, templates:{<tid>:{active,quota}}}` |
| POST | `/api/miniapp/subscribe` | `{accepted_templates:[<tid>...]}` 或 `{unsubscribe_all:true}` |
| GET | `/api/miniapp/notify/templates` | `{template_ids:[...]}`（前端拿去 `wx.requestSubscribeMessage`） |

- `active` = 派生布尔（`quota != 0`）；`quota` = `-1` 长期不限次 / `0` 无 / `>0` 一次性剩余次数（无记录视为 0）。

### 4.7 进出记录 / 运维日志
| 方法 | 路径 | 角色 | 说明 |
|---|---|---|---|
| GET | `/api/miniapp/access-records` | 登录 | query `space_id,date,auth_type(face|card),page,page_size`；响应 `{records:[{name,time,space_id,result,auth_type}], total,page,page_size,has_more}`（脱敏：无 user_id/event_id/人脸图） |
| GET | `/api/miniapp/operation-logs` | admin | `{logs:[...], total,page,page_size,has_more}` |

### 4.8 空间绑定 / 通知绑定 / 门禁白名单（admin）
| 方法 | 路径 | 说明 |
|---|---|---|
| GET | `/api/miniapp/space-bindings` | 教师↔教室绑定列表 |
| POST | `/api/miniapp/space-bindings` | `{user_id, space_id}` |
| DELETE | `/api/miniapp/space-bindings?user_id=&space_id=` | 解绑 |
| GET | `/api/notify/bindings?channel=` | 通知目标绑定列表（channel 必填） |
| POST | `/api/notify/bindings` | `{channel, user_id, external_id, external_extra?}` |
| DELETE | `/api/notify/bindings?channel=&user_id=` | 删除 |
| GET | `/api/miniapp/door-devices?space_id=` | 门禁白名单 |
| POST | `/api/miniapp/door-devices` | `{device_id, space_id, label}` 标记门禁 |
| DELETE | `/api/miniapp/door-devices?space_id=&device_id=` | 取消标记 |

- `/api/notify/bindings` 的 `channel ∈ wechat_miniapp | sms | dingtalk | wecom`（未知 → 400）。这是**通知绑定**渠道枚举，**不含 `wechat_oa`**；家长公众号绑定走独立的 `/api/oa/bind/*` 路径，不属于本枚举。

---

## 5. WebSocket `/ws`（设备状态实时推送）

- 连接：`ws://<host>/ws?token=<JWT>&space_id=<sid>`。**教师必须传已绑定的 `space_id`**（否则握手即被 `forceClose`）；**admin 可省略**（`sid` 为空 = 全部空间）。
- 网关是 **1:1 透明代理**：每个小程序连接对应一条边侧 `/ws`，网关**不过滤任何帧**，边侧每帧原样回推给该客户端；是否广播/按空间由**边侧 go-backend** 决定（外部，不在本仓库）。
- 回推消息类型：
  - `device_update`：`{type, device_id, status(online/offline), app(on/off 或传感器 JSON)}` → 前端按 `device_id` 更新设备在线/离线/开关/环境（**权威状态**）。
  - `cmd_response`：`{type, device_id, command}` → 仅回执提示。**无请求 ID、不保证与某次 control 一一对应**；前端按 `device_id`（+ `space_id`）匹配，忽略不匹配消息。
  - `ping`：边侧心跳，前端忽略；其余未知类型忽略。
- 异步闭环：`POST /api/miniapp/device/control` 200 = 已下发，**最终状态看 `device_update`**，不能以 POST 返回当作执行结果。
- 注意：事件源是边侧 go-backend，网关不生产 `device_update`。

---

## 6. 错误码表（`error.code` 稳定字符串）

| code | HTTP | 含义 |
|---|---|---|
| `INVALID_REQUEST` | 400 | 参数缺失/格式非法 |
| `UNAUTHORIZED` | 401 | 鉴权失败（JWT 无效/过期，或内部令牌错误） |
| `FORBIDDEN` | 403 | 无权限（教师跨空间 / 非 admin 调 admin 接口） |
| `NOT_FOUND` | 404 | 不存在（教师单条告警越权与不存在统一 404，见下） |
| `CONFLICT` | 409 | 冲突（如 code/space_id 已存在） |
| `CONFIRM_REQUIRED` | 428 | 门禁 off 需 `confirm=true` |
| `RATE_LIMITED` | 429 | 频率限制 |
| `GO_BACKEND_UNAVAILABLE` | 503 | 边侧 go-backend 不可达（设备列表 / 设备控制 / 场景 execute 内置 all_on\|all_off） |
| `INTERNAL_ERROR` | 500 | 内部错误 |

- **401 统一处理**：清 token → 重走 `wx.login` → `login`/`bind`。
- **503**：对外 `503` 只有 `GO_BACKEND_UNAVAILABLE`（上述三处）。`UPSTREAM_UNAVAILABLE` 是通用映射码、当前无对外接口发出；`CHANNEL_NOT_CONFIGURED`(503) 仅 `/internal` 家长到校通知，前端不涉及。
- **404 vs 403**：查看类单条告警越权 → **404**（隐藏存在性）；列表带 `space_id` 越权、设备/场景、以及一切操作类 → **403**。

---

## 7. 角色与空间边界（网关鉴权契约，前端按此展示）

| 功能 | 教师 | 管理员 |
|---|---|---|
| 查看设备状态 | 仅本人绑定教室 | 全部教室 |
| 控制设备 / 执行场景 | 仅本人绑定教室 | 全部教室 |
| 查看告警历史 | 本教室 | 全部 |
| 标记告警已处理 | ✗（403） | ✅ |
| 查看进出记录 | 本教室 | 全部 |
| 教师↔教室绑定管理 / 门禁标记 / 教室管理 / 空间类型管理 / 运维日志 | ✗（403） | ✅ |

> 越权区分：查看类越权 404（仅单条告警详情）、操作类越权 403（见 §6 错误码表）。

---

## 8. 修订记录

| 版本 | 日期 | 变更 |
|---|---|---|
| 1.1 | 2026-10-15 | 补异步控制语义与 WS 契约（1:1 透传、cmd_response 按 device_id 匹配、教师 space_id 必传）；补分页默认值/上限与 from/to 格式；`/me` 返回 `name`；补 channel/source 枚举、confirm 类型、alert 状态机与 404/403 区分；503 收敛为 `GO_BACKEND_UNAVAILABLE`；补环境切换与最小可用流程 |
| 1.0 | — | 初稿 |
