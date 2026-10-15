# 边侧 go-backend 对接接口文档

> 网关 = `wechat-gateway`（C++ Drogon，本仓库）；边侧 = `go-backend`（你实现，仓库外，唯一 Go 服务）。

## 关键约定

| 项 | 约定 |
| --- | --- |
| 请求地址 | `base_url + api_prefix + path`；`api_prefix` 默认 `/api`，下文路径均已含 `/api` |
| 设备/控制鉴权 | 网关透传**前端 JWT**（`Authorization: Bearer <JWT>`，HS256，密钥 `JWT_SECRET` 两边同值）。边侧**二选一**：验签 JWT，或信任网关已做边界校验 |
| 手机号查询鉴权 | 请求头 `X-Internal-Token: <网关内部令牌>` |
| 数据权威 | 用户身份 / 设备 / 角色 → **边侧**；`user_spaces`（教师-教室绑定）/ `door_devices`（门禁白名单）/ 告警处理状态 / 运维日志 → **网关 PG**（Go 不建表、不实现） |
| space_id | 所有设备/告警/人脸事件必须带 `space_id`，与 `user_spaces.space_id` 同源同值 |

---

## A. 边侧要实现（网关 → 边侧）

### 登录

**`POST /api/auth/login`**

无需认证。

**请求体：**

```json
{
  "username": "admin",
  "password": "admin123"
}
```

**响应（200）：**

```json
{
  "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
  "user": {
    "user_id": 1,
    "username": "admin",
    "name": "管理员",
    "role": "admin"
  }
}
```

**错误响应：**

| 状态码 | 说明 |
| --- | --- |
| 400 | 用户名或密码为空 |
| 401 | 用户名或密码错误 |

> `user_id` 可为 int 或 string（网关统一转字符串）；`role` 仅 `admin` / `teacher`。

### 获取本空间设备列表

**`GET /api/devices?space_id={space_id}`**

返回指定空间下的所有设备。`space_id` 为必传参数，取综合屏本机的 `space_id`。

**请求参数：**

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `space_id` | string | 是 | 综合屏所在空间 ID，从本机 `fuhe_identity.json` 读取 |

**响应（200）：** 设备对象数组（网关会包装为 `{"devices":[...]}` 给小程序，边侧直接返回数组即可）。

```json
[
  {
    "device_id": "dev_c2936745",
    "device_type": "fuhe-screen",
    "status": "online",
    "app": "on",
    "info": "{}",
    "label": "综合屏",
    "space_id": "spc_38633fd5",
    "last_seen": "2026-09-11T04:52:49Z"
  },
  {
    "device_id": "ZB_aabbcc112233",
    "device_type": "zigbee",
    "status": "online",
    "app": "on",
    "label": "教室灯",
    "zb_type": "switch",
    "gateway_id": "dev_c2936745",
    "space_id": "spc_38633fd5",
    "last_seen": "2026-09-11T04:53:04Z"
  },
  {
    "device_id": "ZB_112233aabbcc",
    "device_type": "zigbee",
    "status": "online",
    "app": "{\"temperature\":25.1,\"humidity\":60}",
    "label": "温湿度传感器",
    "zb_type": "sensor",
    "gateway_id": "dev_c2936745",
    "space_id": "spc_38633fd5",
    "last_seen": "2026-09-11T04:53:04Z"
  }
]
```

**字段说明：**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `device_id` | string | 设备唯一标识 |
| `device_type` | string | 设备类型：`zigbee`（Zigbee 子设备）/ `fuhe-screen`（综合屏）/ `fuhe-board`（电子班牌）/ `fuhe-encoder`（编码器）等 |
| `status` | string | **网络连接状态**：`online` / `offline`。与 `app` 是独立维度，在线不代表开关是开的 |
| `app` | string\|null | **应用层状态**：开关类为 `"on"`/`"off"`；传感器为 JSON 字符串（温湿度、CO₂ 等）；离线时为上次已知状态或 `null` |
| `info` | string\|null | **运行指标**（仅 fuhe 类设备）：JSON 字符串，推流状态、CPU/内存等；无指标为 `{}`；Zigbee 设备无此字段 |
| `label` | string\|null | 设备显示名（管理员可改） |
| `zb_type` | string | 仅 Zigbee 设备：`switch`（开关）/ `sensor`（传感器）/ 空字符串（未分类） |
| `gateway_id` | string | 仅 Zigbee 设备：上级综合屏的 `device_id` |
| `space_id` | string\|null | 所属空间 ID |
| `last_seen` | string\|null | 最后上报时间（UTC ISO 8601） |

**传感器 `app` 数据格式（JSON 字符串）：**

```json
{
  "temperature": 25.1,
  "humidity": 60,
  "co2": 800,
  "illuminance": 300,
  "pressure": 1013,
  "pm25": 12,
  "tvoc": 50,
  "occupancy": true
}
```

传感器只含其支持的字段，不支持的字段不出现。

### 控制设备

**`POST /api/devices/{device_type}/{device_id}/cmd`**

向指定设备下发控制指令。`device_type` 为设备类型，Zigbee 设备传 `zigbee`。

**路径参数：**

| 参数 | 说明 |
| --- | --- |
| `device_type` | 设备类型，如 `zigbee` |
| `device_id` | 目标设备 ID，如 `ZB_aabbcc112233` |

**请求体：**

```json
{
  "command": "on"
}
```

**`command` 可选值：**

| 值 | 说明 |
| --- | --- |
| `on` | 开 |
| `off` | 关 |
| `toggle` | 切换（开→关 / 关→开） |

**响应（200）：**

```json
{
  "status": "ok",
  "topic": "dev_ed1ce280/zigbee/cmd"
}
```

> 响应网关原样透传给小程序。门禁二次确认（`confirm`）在网关侧完成，边侧收到 `command` 直接执行即可。

### 手机号查询

**`GET /api/users/{user_id}`**

请求头：`X-Internal-Token`。短信兜底时解析收件人手机号用。

**响应（200）：**

```json
{ "user_id": "u_admin_1", "phone": "13800000001" }
```

用户不存在返回 `404`。

### 设备实时事件（WebSocket）

**`WS /ws?token=<JWT>&space_id=<sid>`**

网关透传小程序侧 `/ws`（1:1 代理，每条小程序连接 = 一条边侧 WS；`space_id` 空 = 全部空间）。边侧主动广播：

| 消息 | 内容 |
| --- | --- |
| `device_update` | `{ "type":"device_update", "device_id":"...", "status":"online|offline", "app":"on|off|JSON" }` |
| `cmd_response` | `{ "type":"cmd_response", "device_id":"...", "command":"on|off|toggle" }` |
| `ping` | 心跳 |

边侧断线 → 网关关闭对应小程序连接（网关不自动重连边侧），重连由小程序重新发起。

---

## B. 边侧调用网关（边侧 → 网关）

全部 `POST`，请求头 `X-Internal-Token`，路由 `/internal/*`（**不对外**）。

### 告警/事件推送

**`POST /internal/notify/send`**

**请求体：**

```json
{
  "event_type": "device_offline",
  "content": "综合屏离线",
  "event_id": "alt_xxx",
  "space_id": "spc_...",
  "device_id": "dev_...",
  "device_label": "A101 综合屏"
}
```

- `event_type` 仅 4 类：`device_offline` / `sensor_threshold` / `face_login_failed` / `peripheral_offline`。
- **带 `event_id` 才落库**（`alert_id = event_id`），缺省只推送不落库、之后无法处理。
- 触发条件（在线变离线/持续 5 分钟/连续 ≥3 次/离线 30 分钟）由**边侧**判断，网关只做防御性复核；冷却 1800s（键 `event_type+device_id+user+channel`）。

**响应（200）：** `{"ok": true}`。

### 小程序订阅消息单发

**`POST /internal/notify/subscribe-send`**

**请求体：**

```json
{ "openid": "oxxx", "template_id": "tmpl_xxx", "data": { "thing1": "值" } }
```

**响应：** `{"success":true,"channel":"wechat_miniapp","errcode":0,"errmsg":""}`。

### 家长到校通知

**`POST /internal/oa/arrival-notify`**

**请求体：**

```json
{
  "student_no": "20260101",
  "student_name": "张三",
  "space_id": "spc_...",
  "space_name": "A101 教室",
  "arrival_time": "2026-09-11T08:12:00Z"
}
```

**响应：** `{"accepted":true,"final_status":"success|no_parent_binding|skipped|failed|duplicate","channel":"wechat_oa|none"}`。

### 角色全量同步（幂等）

**`POST /internal/user-roles/sync`**

**请求体：**

```json
{ "user_id": "u_teacher_1", "roles": ["teacher"] }
```

`roles` 空数组 = 清空该用户全部角色。同步后网关实时生效，无需重新登录。

### 进出记录回灌（`event_id` 幂等）

**`POST /internal/access-records`**

**请求体：**

```json
{
  "event_id": "evt_xxx",
  "space_id": "spc_...",
  "occurred_at": "2026-09-11T08:12:00Z",
  "name": "张三",
  "auth_type": "face",
  "result": "login"
}
```

### 空间全量同步（⚠️ 必须全量）

**`POST /internal/spaces/sync`**

**请求体：**

```json
{ "spaces": [ { "space_id": "spc_...", "name": "A101 教室", "type": "standard" } ] }
```

**响应：** `{"ok":true,"upserted":1,"removed":0}`。

- 只删「`source=edge` 且未列出」的空间，`source=admin`（管理员建的）**永不删**；被删空间级联清理 `user_spaces`/`door_devices`。
- **传部分列表会真实删除 edge 空间**，务必传全量。

### 花名册全量同步（⚠️ 必须全量）

**`POST /internal/student-parents/sync`**

**请求体：**

```json
{
  "parents": [
    { "student_no": "20260101", "parent_phone": "13800000000", "student_name": "张三", "relation": "父亲", "status": "active" }
  ]
}
```

**响应：** `{"ok":true,"upserted":1,"removed":0}`。

---

## C. 已知缺口（待补，非已实现）

| # | 缺口 | 影响 |
| --- | --- | --- |
| G1 | `user_spaces` 未同步到边侧 face 底库 | 小程序绑定教室后，综合屏刷脸拉不到该教师底库 |
| G2 | 全量同步无 `dry_run`/阈值保护/软删/明细日志 | 传部分列表会删 edge 空间及绑定/门禁标记 |
| G3 | 设备无 `zb_role`/`capabilities` 能力字段 | 场景匹配只能靠 `label` 关键字，存在误/漏匹配 |
