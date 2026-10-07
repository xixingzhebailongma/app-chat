# 差异审计表（综合屏 API）

> 状态：待确认。**关键前提**——本机 `/home/kai` 下没有 go-backend / sip-service / Edge Agent 的源码（已全盘搜索：无 go.mod、无 `gw-unassigned`/`p2p_mode`/`sip_status` 等标记），后端源码在边侧机器（192.168.102.13）或独立仓库。因此下表「建议修改文件/函数」只能按**角色**定位，无法给出真实文件/函数名，待拿到后端源码后补全。

验收命令统一约定：
- `B=http://192.168.102.13:30801; TOKEN=$(curl -s -X POST $B/api/auth/login -H 'Content-Type: application/json' -d '{"username":"admin","password":"admin123"}' | python3 -c 'import sys,json;print(json.load(sys.stdin)["token"])')`
- 每项另附 `check_edge_api.py` 中对应检查 ID，修复后该 ID 应从 FAIL → PASS。

---

## P0-1  GET /provision

| 项 | 内容 |
|---|---|
| **接口** | `GET /provision?space_id={space_id}` |
| **文档期望** | 返回 `server, port, extension, name, auto_answer, center_ext, multicast_addr, ipc` |
| **实际行为** | 有 `server/port/name/multicast_addr/ipc`；缺 `extension/auto_answer/center_ext`；多出 `p2p_mode/space_id`。示例：`{"ipc":[],"multicast_addr":"239.0.0.1:5004","name":"测试空间","p2p_mode":true,"port":5060,"server":"192.168.102.13","space_id":"spc_38633fd5"}` |
| **根因（推断）** | sip-service 的 provision handler 已按 P2P 模式改造（新增 `p2p_mode`），分机号/自动接听/中控分机字段被移除或改名，但接口契约未同步。需确认 DB 是否仍存分机号、字段名是 `extension` 还是 `ext`。 |
| **建议修改文件/函数** | sip-service：`GET /provision` 的 handler + 响应 DTO（**源码不在本机，待定位**） |
| **风险** | 若 DB 已无分机号数据，补字段需先确认数据来源；改错会破坏 SIP 注册/拨号。 |
| **验收命令** | `curl -s "$B/provision?space_id=spc_38633fd5"` 应含 `extension/auto_answer/center_ext`；`check_edge_api.py` 的 `provision_shape` → PASS |

## P0-2  GET /provision/sip/endpoints

| 项 | 内容 |
|---|---|
| **接口** | `GET /provision/sip/endpoints[?refresh=1]` |
| **文档期望** | 裸结构 `{ "center": {type,ext,name,ip,space_id,sip_status,last_seen}, "classrooms":[{同上}] }`，不被 envelope 包裹；refresh=1 走 Asterisk AMI 实时查询 |
| **实际行为** | 被 `{data,message,status}` 包裹；**无 `center`**；classroom 对象为 `{type,name,ip,space_id,sip_status,last_seen}`，**缺 `ext`**；多 `sip_port`。示例：`{"data":{"classrooms":[{"type":"classroom","name":"test","ip":"192.168.102.127","space_id":"spc_d9d40ca1","sip_status":"online","last_seen":""},...],"sip_port":5060},"message":"ok","status":200}` |
| **根因（推断）** | endpoints handler 用统一 envelope 包装；未单独构造 `center` 对象；classroom 序列化未含 `ext`（分机号）。 |
| **建议修改文件/函数** | sip-service：endpoints handler + classroom/center DTO（**源码不在本机，待定位**） |
| **风险** | 通讯录缺 `ext` 无法拨号；去掉 envelope 可能影响其它依赖 sip-service 的消费者（需评估）。 |
| **验收命令** | `curl -s "$B/provision/sip/endpoints"` 顶层应含 `center` 与 `classrooms[]`，且 `classrooms[].ext` 非空；`check_edge_api.py` 的 `endpoints_shape` → PASS |

## P0-3  GET /provision/ipc

| 项 | 内容 |
|---|---|
| **接口** | `GET /provision/ipc?space_id={space_id}` |
| **文档期望** | `{ name, ext, ipc[] }` |
| **实际行为** | `{ name, ipc[], space_id }`，缺 `ext`。示例：`{"ipc":[],"name":"测试空间","space_id":"spc_38633fd5"}` |
| **根因（推断）** | ipc handler 响应结构缺 `ext` 字段。 |
| **建议修改文件/函数** | sip-service：ipc handler + DTO（**源码不在本机，待定位**） |
| **风险** | 低（仅缺显示用分机号）。 |
| **验收命令** | `curl -s "$B/provision/ipc?space_id=spc_38633fd5"` 应含 `ext`；`check_edge_api.py` 的 `ipc_shape` → PASS |

## P0-4  GET /api/devices（space_id 未强制 / 无授权过滤）

| 项 | 内容 |
|---|---|
| **接口** | `GET /api/devices?space_id={space_id}` |
| **文档期望** | `space_id` 必传；按 token 权限过滤：教师只能看自己绑定教室，管理员可看指定/全局 |
| **实际行为** | 不传 `space_id` 返回全服务端设备（HTTP 200，6 台跨 2 空间）；无角色/空间授权过滤 |
| **根因（推断）** | go-backend 的 devices handler 未校验 `space_id` 参数，也未按 JWT 的 `role`/绑定空间做授权过滤。 |
| **建议修改文件/函数** | go-backend：devices handler + 授权中间件/JWT 解析（**源码不在本机，待定位**） |
| **风险** | 跨空间数据泄露；需先明确 teacher 的空间绑定关系来源（JWT claims 或 DB）。 |
| **验收命令** | 不传 `space_id` → 400；教师 token 仅返回其授权空间；admin 可看指定/全局；`check_edge_api.py` 的 `devices_no_space` 需把期望从「未过滤」改为「应 400」后 → PASS |

## P0-5  Zigbee 控制假成功（gw-unassigned）

| 项 | 内容 |
|---|---|
| **接口** | `POST /api/devices/zigbee/{device_id}/cmd` |
| **文档期望** | 400=command 非法或设备未绑定网关；404=设备不存在；有效时 200 `{"status":"ok","topic":"dev_ed1ce280/zigbee/cmd"}` 且随后有 `cmd_response`、`device_update` |
| **实际行为** | 对 `gateway_id=gw-unassigned` 的 `ZB_0xae87` 下发 toggle 返回 `HTTP 200 {"status":"ok","topic":"gw-unassigned/zigbee/cmd"}`；无 `cmd_response`，`device_update` 状态不变（off 仍 off） |
| **根因（推断）** | cmd handler 仅校验 command(400)/存在(404)，未校验 `gateway_id` 有效且非 `gw-unassigned`、网关在线/有 MQTT 订阅，直接把「已 publish」当「成功」返回 200。 |
| **建议修改文件/函数** | go-backend：zigbee cmd handler（**源码不在本机，待定位**） |
| **风险** | 控制类接口假成功误导用户；需明确「已下发」vs「已执行」语义（网关离线/未绑定应返回 4xx 或 502/503）。 |
| **验收命令** | 对 `gw-unassigned` 设备 toggle → 400/404（非 200）；对有效网关设备 toggle → 200 + 真 topic + 收到 `cmd_response`/`device_update`。`check_edge_api.py` 新增对应断言 |

## P1-6  Edge Agent 语音服务 :30909 宕机

| 项 | 内容 |
|---|---|
| **接口** | `POST :30909/chat` |
| **文档期望** | 无 token 401 / 空 text 400 / 正常 200 `{reply,actions}` / `space_id` 缺失时不误操作其它教室 |
| **实际行为** | 测试期间从「正常返回 400/401」变为 `Connection refused`（3 次重试均拒绝） |
| **根因** | Edge Agent 进程停止/崩溃（远端，需查进程/端口/日志）。 |
| **建议修改文件/函数** | 运维恢复服务，非代码修改；恢复后补测 |
| **风险** | 语音控制整体不可用 |
| **验收命令** | 恢复后：`curl -s -X POST http://192.168.102.13:30909/chat` 各用例 + `check_edge_api.py` 的 `chat_empty/chat_noauth` 由 SKIP → PASS；`RUN_MUTATING=1` 补 `mut_chat`（「把灯打开」→ 200 + reply/actions + WS device_update） |

---

## P1-7 中低问题（以文档为准 / 列为待确认，不擅自补协议）

| 项 | 接口 | 文档 vs 实际 | 建议 |
|---|---|---|---|
| 7a | 全接口错误体 | 文档未定义；实际 4 种格式（`detail` / `error` / `{error,message,status}` / `{error,hint,seen_ip}`） | 统一为一种可读错误结构（如 `{error:{code,message}}`），不泄露内部信息 |
| 7b | `POST /api/face/login` 缺参 | 文档「400 特征维度错误或缺少必要参数」；实际泄露 `Key: 'Feature' Error:Field validation...` | 缺参返回干净文案（对齐 7a），不透传 Go validator |
| 7c | `/sip/api/settings` | 文档「SIP 所有接口无需认证」；实际 401 | **待确认**：SIP 管理 API 是否需要认证，以文档或后端定义为准 |
| 7d | 设备 ID 格式 | HTTP 示例 `ZB_aabbcc112233`；离线文件 `ZB_0x011b`；线上实际 `ZB_0xd613` | **待确认映射关系**，不改动，仅修正文档示例 |
| 7e | 未定义消息/指令 | `face_login`/`agent_event`/`space_delete` 无 schema；`set_temperature` 未定义 | **列为待确认**，不自行补协议 |

---

## 回归运行方式（后端修复后自测）

```bash
cd api-tests
# 只读回归（目标 0 FAIL / 0 SKIP，30909 恢复后）
RUN_MUTATING=0 python3 check_edge_api.py
# 变更性回归（目标 mut_gw_unassigned PASS；有在线网关时 mut_valid_gw 观察到 cmd_response/device_update）
RUN_MUTATING=1 python3 check_edge_api.py
```

当前 FAIL 项与修复点的对应（`check_edge_api.py` 断言 ID → 本文条目）：

| 断言 ID | 对应条目 | 修复后预期 |
|---|---|---|
| `provision_shape` | P0-1 | PASS（含 extension/auto_answer/center_ext） |
| `endpoints_shape` | P0-2 | PASS（裸 {center, classrooms[].ext}） |
| `ipc_shape` | P0-3 | PASS（含 ext） |
| `devices_no_space` | P0-4 | PASS（不传 space_id → 400） |
| `teacher_filter` | P0-4 | PASS（教师 token 不泄露非绑定空间） |
| `mut_gw_unassigned` | P0-5 | PASS（gw-unassigned 设备控制被 400/404 拒绝） |
| `chat_empty` / `chat_noauth` / `mut_chat` | P1-6 | 30909 恢复后 SKIP → PASS |

---

## 阻塞项与所需信息

1. **后端源码不在本机**（go-backend / sip-service / Edge Agent），`/home/kai` 全盘无匹配。本次按「后端他人改、我只维护验收」执行：审计表 + 验收脚本已交付，后端按本文档修复后自测即可。
2. **`gw-unassigned` 的产生来源**：是装网关时的默认占位，还是设备离网时的中间态？影响 P0-5 的校验策略（报 4xx 还是仅提示）。
3. **教师空间的绑定关系**（P0-4 授权过滤的数据来源）：JWT 是否已带 `space_id`/角色？还是需 DB 查询。
