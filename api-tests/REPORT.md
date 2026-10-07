# 综合屏 API 对接测试报告

> 测试目标：对照《综合屏 API 对接文档》逐项验证线上 go-backend / sip-service / Edge Agent 的实际行为。
> 测试方式：只读探测 + WebSocket 检查 + 变更性测试（真实控制指令，已获授权）。
> 生成时间：2026-09-22（UTC）

## 测试环境

| 项 | 值 |
|---|---|
| go-backend（nginx 入口） | `http://192.168.102.13:30801`（`/api/*`） |
| Edge Agent 语音服务 | `http://192.168.102.13:30909`（`/chat`） |
| 登录账号 | `admin` / `admin123` |
| 发现的真实 space_id | `spc_38633fd5`（测试空间）、`spc_d9d40ca1`（test） |
| 文档示例 space_id | `spc_a8acdd5c`（实际不存在，仅为占位） |

> 说明：文档中多处示例的 `space_id`/`device_id` 为虚构值（如 `spc_a8acdd5c`、`ZB_aabbcc112233`），本报告一律以线上真实数据为准。

---

## 结论摘要

- **阻断级问题 3 项**：SIP 相关接口（`/provision`、`/provision/sip/endpoints`、`/provision/ipc`）返回结构/字段与文档严重不符，按文档实现 SIP 对讲/通讯录/拨号无法落地。
- **中等问题 6 项**：space_id 过滤未强制、设备 ID 格式文档写错、`gw-unassigned` 哨兵值、错误响应体约定不统一、SIP 管理接口认证要求与文档矛盾、Edge Agent 服务不稳定。
- **轻微 / 文档内部矛盾若干**。
- 其余接口（登录、设备列表字段模型、课表、人脸/刷卡底库、控制指令错误码、`/chat` 错误码、WebSocket 鉴权与快照）**与文档一致**。

---

## 一、阻断级问题

### 1. `GET /provision` 缺少 SIP 注册所需字段（extension / auto_answer / center_ext）

文档声明返回 `server、port、extension、name、auto_answer、center_ext、multicast_addr、ipc`，并据此描述「用 extension 向 Asterisk 注册 + 拨 center_ext」的完整对讲流程。

**实际返回**（`GET /provision?space_id=spc_38633fd5`）：

```json
{"ipc":[],"multicast_addr":"239.0.0.1:5004","name":"测试空间","p2p_mode":true,"port":5060,"server":"192.168.102.13","space_id":"spc_38633fd5"}
```

- 缺失：`extension`、`auto_answer`、`center_ext`
- 多出（未文档化）：`p2p_mode`、`space_id`

**影响**：文档第五章「SIP 注册方式」所需的分机号与中控分机号拿不到，教室端无法按文档注册/拨号。`p2p_mode` 字段暗示后端已转向 P2P 模式，**文档的 Asterisk 注册流程已过时**。

### 2. `GET /provision/sip/endpoints` 结构完全不符（缺 center、缺 ext、多 envelope）

文档声明返回裸结构 `{ "center": {...}, "classrooms": [{ type, ext, name, ip, space_id, sip_status, last_seen }] }`，且明确「取对方 `ext` 字段直接拨号」。

**实际返回**：

```json
{"data":{"classrooms":[
  {"type":"classroom","name":"test","ip":"192.168.102.127","space_id":"spc_d9d40ca1","sip_status":"online","last_seen":""},
  {"type":"classroom","name":"测试空间","ip":"192.168.102.10","space_id":"spc_38633fd5","sip_status":"online","last_seen":"2026-09-07T08:43:14Z"}
],"sip_port":5060},"message":"ok","status":200}
```

- 被 `{data, message, status}` 信封包裹（文档为裸结构）
- **没有 `center` 对象**
- 教室对象**缺少 `ext`（分机号）** → 「取对方 ext 拨号」无法执行
- 多出未文档化的 `sip_port`

### 3. `GET /provision/ipc` 缺少 `ext`

文档声明返回 `{ name, ext, ipc }`。**实际返回**（`GET /provision/ipc?space_id=spc_38633fd5`）：

```json
{"ipc":[],"name":"测试空间","space_id":"spc_38633fd5"}
```

缺失 `ext`，多出 `space_id`。

---

## 二、中等问题

### 4. `GET /api/devices` 的 `space_id` 并非「必传」，未做服务端过滤

文档明确「`space_id` 为必传参数」。**实际**不传 `space_id` 时返回 HTTP 200 与**全部空间**的设备：

```
GET /api/devices（无 space_id，带 token）→ HTTP 200，返回 6 台，跨 2 个空间 [spc_38633fd5, spc_d9d40ca1]
```

**影响**：综合屏（或任何持有 token 的客户端）只要省略参数即可拿到其它教室的设备，存在跨空间数据泄露；过滤完全依赖客户端自觉，与「必传」表述矛盾。

### 5. 设备 ID 格式：文档 HTTP 示例写错

- 文档 `GET /api/devices` 示例：`ZB_aabbcc112233`、`ZB_112233aabbcc`（12 位十六进制）。
- **线上实际**：`ZB_0xd613`、`ZB_0x0b34`、`ZB_0xae87`（`ZB_0x` + 4 位十六进制）。
- 文档「离线本地控制」章节与「语音指令」章节已用 `ZB_0x{4位}` 正确格式，**仅 HTTP devices 示例写错**。控制接口 `/devices/{type}/{id}/cmd` 的路径参数应取实际 `device_id`。

### 6. `gateway_id` 存在哨兵值 `"gw-unassigned"`，且会静默吞掉控制指令

文档只说明 `gateway_id` 为「所属网关 ID（上级综合屏的 device_id）」，未提及特殊值。**线上实际**：spc_d9d40ca1 的 `ZB_0xae87` 返回 `"gateway_id":"gw-unassigned"`。

**实际后果**（变更性测试）：向该设备下发 `toggle` 返回 `HTTP 200 {"status":"ok","topic":"gw-unassigned/zigbee/cmd"}` —— 指令被发布到 `gw-unassigned/zigbee/cmd` 这个**无网关订阅的死主题**，设备不会执行，且无 `cmd_response` 回执。即：对这类设备操作「看起来成功、实际无效果」。

### 7. 错误响应体约定不统一（4 种格式）

| 服务 | 示例 | 错误体字段 |
|---|---|---|
| go-backend `/api/*` | 401 未登录 | `{"detail":"未登录或 token 已过期"}` |
| Edge Agent `/chat` | 400 空文本 | `{"error":"text 不能为空"}` |
| sip-service `/provision` 404 | 未装机 | `{"error":"...","hint":"...","seen_ip":"10.42.0.1"}` |
| sip-service `/sip/api/*` | 401 | `{"error":"...","message":"...","status":401}` |

文档未说明任何错误体格式，客户端需分别兼容 `detail` / `error` / `{error,message,status}` 三种键。此外 `/provision` 404 返回的 `seen_ip` 是 nginx 代理后的内网 IP（`10.42.0.1`），对排查帮助有限。

### 8. `/sip/api/settings` 实际需要认证，与「SIP 所有接口无需认证」矛盾

文档 SIP 章节称「所有接口无需认证，综合屏直接调用」。**实际** `GET /sip/api/settings` 返回 HTTP 401：

```json
{"error":"未登录或登录已过期","message":"未登录或登录已过期","status":401}
```

SIP 管理 API（`/sip/api/*`）需要登录，教室端取配置接口（`/provision*`）才无需认证——文档的「所有接口」表述范围过宽。

### 9. Edge Agent 语音服务（30909）在测试期间宕机

测试开始约 15 分钟时，`POST :30909/chat` 尚能正常返回 400/401（空文本、无 token 均按文档正确拒绝）；此后三次重试均为 `Connection refused`。`/chat` 空文本与无 token 两条用例因此转为 SKIP。

**影响**：无法完成语音指令的真实端到端验证（下发「把灯打开」观察 `reply`/`actions`）。需确认 Edge Agent 服务是否被重启、是否有稳定性问题。

---

## 三、轻微问题

### 10. `face/login` 缺参错误泄露框架内部信息

`POST /api/face/login` 缺 `feature` 时返回：

```json
{"detail":"参数错误：Key: 'Feature' Error:Field validation for 'Feature' failed on the 'required' tag"}
```

而「特征维度错误」返回的是干净文案 `特征维度错误，期望 512 维`。缺参场景直接透传 Go validator 内部错误，不一致且对客户端不友好。

### 11. 时间格式与示例值的小差异

- `last_seen` 实际带微秒：`2026-09-11T04:39:52.82106Z`（文档示例无小数秒 `...04:52:49Z`）。均为 ISO 8601，客户端解析需容忍小数秒。
- 登录 `user.name` 实际为「超级管理员」（文档示例「管理员」），属占位示例，非问题。

---

## 四、文档内部矛盾（无法线上验证，仅供修订文档参考）

1. **路由表列出了 `face_login` / `agent_event` / `space_delete` 消息，但「消息类型」章节未给出它们的 schema**，客户端无从得知字段结构。
2. **语音示例动作含 `set_temperature`**（「空调调到26度」→ `zigbee ZB_0xb123 → set_temperature`），但控制接口 `command` 仅 `on/off/toggle`、离线 FIFO 仅 `setState/getState/permitJoin/removeDevice`，`set_temperature` 在整套文档中未定义。
3. **无在线开放 Zigbee 入网的 HTTP 接口**：文档有 `permit_join_status` WS 消息与离线 FIFO 的 `permitJoin`，但缺少在线（有网）场景下触发入网的接口，链路不完整。
4. 控制接口示例 `POST /api/devices/{device_type}/{device_id}/cmd` 中，路径 `device_id` 用 `ZB_aabbcc112233`，但响应 `topic` 示例却是 `dev_ed1ce280/zigbee/cmd`（网关 ID），易误导。

---

## 五、已验证一致（符合文档）

- `GET /api/health` → `{"status":"ok"}`
- `POST /api/auth/login`：正确凭据 200 + `token` + `user{user_id(int)/username/name/role}`；空 body 400；错密码 401
- `GET /api/devices?space_id=`：字段模型（`device_id/device_type/status/app/info(仅 fuhe)/zb_type/gateway_id/label/space_id/last_seen`）正确；`info` 仅 fuhe 类有、Zigbee 无；无 token 401
- `GET /api/board/schedules`：`entries[].weekday/period/subject/start/end` 齐全；无 token 401
- `GET /api/face/gallery`：`teachers[].feature` 512 维；无 token 401
- `GET /api/card/gallery`：`teachers[].user_id/username/name/rf_id` 齐全
- `POST /api/devices/zigbee/{id}/cmd`：非法 command 400、设备不存在 404、无 token 401
- `POST /api/face/login`：10 维特征 400；缺参 400
- `POST /api/card/login`：未知卡号 401
- `POST /api/card/bind`：错密码 401
- WebSocket `/ws`：坏 token / 无 token 均 401 拒绝；带 space_id 收到 `device_update` 快照（含 `device_id/device_type/status/app/space_id`）；不带 space_id 能连（收全量）；30s 内收到 `ping`
- `POST /chat`：空文本 400、无 token 401（服务在线时）

---

## 六、变更性测试结果

| 操作 | 结果 |
|---|---|
| `POST /devices/zigbee/ZB_0xae87/cmd {"command":"toggle"}` | `HTTP 200 {"status":"ok","topic":"gw-unassigned/zigbee/cmd"}` |
| 等待 WebSocket 回执 | 未收到 `cmd_response`；仅收到初始快照的 `device_update`（`status:"offline", app:"off"`，状态未变） |
| `POST /chat {"text":"把灯打开"}` | 服务不可达（30909 宕机），未完成 |

**解读**：控制接口的「200 = 已下发 MQTT，不代表设备执行」得到印证——因目标设备 `gateway_id` 为 `gw-unassigned` 且处于 offline，指令发布到死主题，无回执、无状态变化。这与问题 6 相互印证。

---

## 附：复现方式

```bash
cd api-tests
python3 check_edge_api.py                 # 完整回归（含变更性测试）
RUN_MUTATING=0 python3 check_edge_api.py  # 仅只读回归，不触碰设备
```

可配置环境变量：`BASE_URL`、`CHAT_URL`、`ADMIN_USER`、`ADMIN_PASS`、`RUN_MUTATING`、`CHECK_PING`、`TIMEOUT`。结果写入 `results.json`。

- 测试脚本：`check_edge_api.py`
- 结果数据：`results.json`
