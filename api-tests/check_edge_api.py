#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
综合屏 API 对接测试脚本

针对「综合屏 API 对接文档」的接口做回归检查：只读断言 + WebSocket 检查
+ 可选的变更性测试（真实控制指令）。输出 [PASS]/[FAIL]/[SKIP]/[INFO]
并落一份 results.json。

环境变量（均可选）：
  BASE_URL       go-backend nginx 入口，默认 http://192.168.102.13:30801
  CHAT_URL       Edge Agent 语音服务，默认 http://192.168.102.13:30909
  ADMIN_USER / ADMIN_PASS   admin 账号，默认 admin / admin123
  RUN_MUTATING   是否下发真实控制指令（toggle + /chat），默认 1
  CHECK_PING     是否等待 WebSocket 30s 心跳 ping，默认 1
  TIMEOUT        HTTP 超时秒数，默认 8
"""

import os
import sys
import json
import time
import asyncio
import urllib.request
import urllib.error

try:
    import websockets  # type: ignore
except Exception:  # pragma: no cover
    websockets = None

BASE_URL = os.environ.get("BASE_URL", "http://192.168.102.13:30801").rstrip("/")
CHAT_URL = os.environ.get("CHAT_URL", "http://192.168.102.13:30909").rstrip("/")
ADMIN_USER = os.environ.get("ADMIN_USER", "admin")
ADMIN_PASS = os.environ.get("ADMIN_PASS", "admin123")
RUN_MUTATING = os.environ.get("RUN_MUTATING", "1") == "1"
CHECK_PING = os.environ.get("CHECK_PING", "1") == "1"
TIMEOUT = float(os.environ.get("TIMEOUT", "8"))

RESULTS = []


def record(cid, name, status, actual="", expected=""):
    """记录一条检查结果并打印。status ∈ pass/fail/skip/info。"""
    RESULTS.append({"id": cid, "name": name, "status": status,
                    "actual": actual, "expected": expected})
    mark = {"pass": "PASS", "fail": "FAIL", "skip": "SKIP", "info": "INFO"}[status]
    print(f"[{mark}] {name}")
    if status == "fail":
        print(f"        期望: {expected}")
        print(f"        实际: {actual}")
    elif status in ("info", "skip") and actual:
        print(f"        {actual}")


def _try_json(s):
    try:
        return json.loads(s)
    except Exception:
        return None


def request(method, url, token=None, body=None, timeout=None):
    """返回 (status, raw_text, json_or_None)。网络异常时 status 为 None。"""
    t = timeout or TIMEOUT
    data = None
    headers = {"Accept": "application/json"}
    if body is not None:
        data = json.dumps(body).encode("utf-8")
        headers["Content-Type"] = "application/json"
    if token:
        headers["Authorization"] = "Bearer " + token
    req = urllib.request.Request(url, data=data, headers=headers, method=method)
    try:
        with urllib.request.urlopen(req, timeout=t) as r:
            raw = r.read().decode("utf-8", "replace")
            return r.status, raw, _try_json(raw)
    except urllib.error.HTTPError as e:
        raw = e.read().decode("utf-8", "replace")
        return e.code, raw, _try_json(raw)
    except Exception as e:  # 连接失败 / 超时
        return None, f"<exception: {e}>", None


def short(s, n=180):
    s = str(s)
    return s if len(s) <= n else s[:n] + f"...({len(s)} chars)"


# --------------------------------------------------------------------------
# 只读断言
# --------------------------------------------------------------------------
def read_only_checks():
    # 1. 健康检查
    st, raw, j = request("GET", f"{BASE_URL}/api/health")
    record("health", "GET /api/health 返回 200 且 {status:ok}",
           "pass" if (st == 200 and isinstance(j, dict) and j.get("status") == "ok")
           else "fail", f"HTTP {st} {short(raw)}", "HTTP 200 {'status':'ok'}")

    # 2. 登录：正确凭据
    st, raw, j = request("POST", f"{BASE_URL}/api/auth/login",
                         body={"username": ADMIN_USER, "password": ADMIN_PASS})
    token = None
    if st == 200 and isinstance(j, dict) and isinstance(j.get("token"), str) and j["token"]:
        u = j.get("user") or {}
        ok = all(k in u for k in ("user_id", "username", "name", "role")) \
            and isinstance(u.get("user_id"), int)
        token = j["token"]
        record("login_ok", "POST /api/auth/login 正确凭据返回 token + user(user_id/username/name/role)",
               "pass" if ok else "fail",
               f"HTTP {st} user={u}", "200 且 user 含 user_id(int)/username/name/role")
    else:
        record("login_ok", "POST /api/auth/login 正确凭据",
               "fail", f"HTTP {st} {short(raw)}", "HTTP 200 + token")

    # 3. 登录：空 body
    st, raw, j = request("POST", f"{BASE_URL}/api/auth/login", body={})
    record("login_empty", "POST /api/auth/login 空 body 返回 400",
           "pass" if st == 400 else "fail", f"HTTP {st} {short(raw)}", "HTTP 400")

    # 4. 登录：错误密码
    st, raw, j = request("POST", f"{BASE_URL}/api/auth/login",
                         body={"username": ADMIN_USER, "password": "definitely_wrong"})
    record("login_bad", "POST /api/auth/login 错误密码返回 401",
           "pass" if st == 401 else "fail", f"HTTP {st} {short(raw)}", "HTTP 401")

    if not token:
        record("discover", "无法获取 token，后续鉴权检查跳过", "skip")
        return None

    # 发现真实 space_id（文档说从 fuhe_identity.json 读，这里从 SIP endpoints 自动发现）
    space_ids = []
    st, raw, j = request("GET", f"{BASE_URL}/provision/sip/endpoints")
    if isinstance(j, dict) and isinstance(j.get("data"), dict):
        for c in j["data"].get("classrooms") or []:
            sid = c.get("space_id")
            if sid and sid not in space_ids:
                space_ids.append(sid)
    if not space_ids:
        space_ids = ["spc_38633fd5", "spc_d9d40ca1"]
    sid = space_ids[0]
    record("discover", f"发现 space_id: {space_ids}", "info", str(space_ids))

    # 5. 设备列表（鉴权 + 字段模型）
    st, raw, j = request("GET", f"{BASE_URL}/api/devices?space_id={sid}", token=token)
    devices = j if isinstance(j, list) else None
    if st == 200 and isinstance(devices, list):
        missing = []
        for d in devices:
            for k in ("device_id", "device_type", "status"):
                if k not in d:
                    missing.append(f"{d.get('device_id','?')}.{k}")
        record("devices_shape", f"GET /api/devices?space_id= 返回设备列表且字段完整 ({len(devices)} 台)",
               "pass" if not missing else "fail",
               f"{len(devices)} 台, 缺字段: {missing[:5]}", "每台含 device_id/device_type/status")
    else:
        record("devices_shape", "GET /api/devices 返回列表", "fail",
               f"HTTP {st} {short(raw)}", "HTTP 200 + list")

    # 6. 设备列表：无 token
    st, raw, j = request("GET", f"{BASE_URL}/api/devices?space_id={sid}")
    record("devices_noauth", "GET /api/devices 无 token 返回 401",
           "pass" if st == 401 else "fail", f"HTTP {st} {short(raw)}", "HTTP 401")

    # 7. 设备列表：不传 space_id —— 文档称“必传”，验收目标：返回 400
    st, raw, j = request("GET", f"{BASE_URL}/api/devices", token=token)
    record("devices_no_space", "GET /api/devices 不传 space_id（必传）返回 400",
           "pass" if st == 400 else "fail", f"HTTP {st} {short(raw)}",
           "HTTP 400（space_id 必传，不传应被拒绝）")

    # 8. 设备列表：不存在的 space_id
    st, raw, j = request("GET", f"{BASE_URL}/api/devices?space_id=spc_nonexistent", token=token)
    record("devices_bad_space", "GET /api/devices 不存在的 space_id 返回空列表",
           "pass" if (st == 200 and j == []) else "info", f"HTTP {st} {short(raw)}",
           "HTTP 200 []")

    # 9. 课表
    st, raw, j = request("GET", f"{BASE_URL}/api/board/schedules?space_id={sid}", token=token)
    if st == 200 and isinstance(j, dict) and isinstance(j.get("entries"), list):
        e0 = (j["entries"][0] if j["entries"] else {})
        ok = all(k in e0 for k in ("weekday", "period", "subject", "start", "end")) \
            if j["entries"] else True
        record("schedules", f"GET /api/board/schedules 返回 entries 且字段完整 ({len(j['entries'])} 条)",
               "pass" if ok else "fail", f"{len(j['entries'])} 条, 首条={e0}",
               "entries[].weekday/period/subject/start/end")
    else:
        record("schedules", "GET /api/board/schedules", "fail",
               f"HTTP {st} {short(raw)}", "HTTP 200 {space_id, entries[]}")

    # 10. 课表：无 token
    st, raw, j = request("GET", f"{BASE_URL}/api/board/schedules?space_id={sid}")
    record("schedules_noauth", "GET /api/board/schedules 无 token 返回 401",
           "pass" if st == 401 else "fail", f"HTTP {st} {short(raw)}", "HTTP 401")

    # 11. 人脸底库
    st, raw, j = request("GET", f"{BASE_URL}/api/face/gallery?space_id={sid}", token=token)
    if st == 200 and isinstance(j, dict) and isinstance(j.get("teachers"), list):
        dims = [len(t.get("feature") or []) for t in j["teachers"] if isinstance(t.get("feature"), list)]
        bad = [d for d in dims if d != 512]
        record("face_gallery", f"GET /api/face/gallery 返回底库，feature 512 维 ({len(j['teachers'])} 人)",
               "pass" if not bad else "fail",
               f"{len(j['teachers'])} 人, 维度={dims}", "teachers[].feature 512 维")
    else:
        record("face_gallery", "GET /api/face/gallery", "fail",
               f"HTTP {st} {short(raw)}", "HTTP 200 {space_id, teachers[]}")

    # 12. 人脸底库：无 token
    st, raw, j = request("GET", f"{BASE_URL}/api/face/gallery?space_id={sid}")
    record("face_gallery_noauth", "GET /api/face/gallery 无 token 返回 401",
           "pass" if st == 401 else "fail", f"HTTP {st} {short(raw)}", "HTTP 401")

    # 13. 刷卡底库
    st, raw, j = request("GET", f"{BASE_URL}/api/card/gallery?space_id={sid}", token=token)
    if st == 200 and isinstance(j, dict) and isinstance(j.get("teachers"), list):
        ok = all(all(k in t for k in ("user_id", "username", "name", "rf_id"))
                 for t in j["teachers"])
        record("card_gallery", f"GET /api/card/gallery 返回底库且字段完整 ({len(j['teachers'])} 人)",
               "pass" if ok else "fail", f"{len(j['teachers'])} 人", "teachers[].user_id/username/name/rf_id")
    else:
        record("card_gallery", "GET /api/card/gallery", "fail",
               f"HTTP {st} {short(raw)}", "HTTP 200 {space_id, teachers[]}")

    # 13b. 教师 token 授权过滤（P0-4 目标：教师只能看自己绑定教室）
    st, raw, j = request("GET", f"{BASE_URL}/api/card/gallery?space_id={sid}", token=token)
    teacher_rf = None
    if st == 200 and isinstance(j, dict) and isinstance(j.get("teachers"), list) and j["teachers"]:
        teacher_rf = j["teachers"][0].get("rf_id")
    other_sid = space_ids[1] if len(space_ids) > 1 else None
    if teacher_rf and other_sid:
        st, raw, j = request("POST", f"{BASE_URL}/api/card/login",
                             body={"rf_id": teacher_rf, "space_id": sid})
        ttoken = (j or {}).get("token") if st == 200 else None
        if ttoken:
            st2, raw2, j2 = request("GET", f"{BASE_URL}/api/devices?space_id={other_sid}", token=ttoken)
            leak = (st2 == 200 and isinstance(j2, list) and
                    any(d.get("space_id") == other_sid for d in j2))
            record("teacher_filter", "教师 token 请求非绑定空间不应泄露设备",
                   "pass" if not leak else "fail",
                   f"HTTP {st2} 返回 {short(j2) if isinstance(j2, list) else raw2}",
                   f"非绑定空间 {other_sid} 的设备应被过滤（403 或空列表），不应返回")
        else:
            record("teacher_filter", "获取教师 token（card/login）", "skip",
                   f"card/login 失败 HTTP {st} {short(raw)}")
    else:
        record("teacher_filter", "教师授权过滤检查", "skip",
               "无可用教师 rf_id 或仅 1 个空间")

    # 14. 控制指令错误路径
    zig = next((d for d in (devices or []) if d.get("device_type") == "zigbee"), None)
    zig_id = zig["device_id"] if zig else "ZB_0x0000"
    st, raw, j = request("POST", f"{BASE_URL}/api/devices/zigbee/{zig_id}/cmd",
                         token=token, body={"command": "blah"})
    record("cmd_invalid", "POST .../cmd 非法 command 返回 400",
           "pass" if st == 400 else "fail", f"HTTP {st} {short(raw)}", "HTTP 400")

    st, raw, j = request("POST", f"{BASE_URL}/api/devices/zigbee/ZB_nonexistent/cmd",
                         token=token, body={"command": "on"})
    record("cmd_notfound", "POST .../cmd 不存在的设备返回 404",
           "pass" if st == 404 else "fail", f"HTTP {st} {short(raw)}", "HTTP 404")

    st, raw, j = request("POST", f"{BASE_URL}/api/devices/zigbee/{zig_id}/cmd",
                         body={"command": "on"})
    record("cmd_noauth", "POST .../cmd 无 token 返回 401",
           "pass" if st == 401 else "fail", f"HTTP {st} {short(raw)}", "HTTP 401")

    # 15. SIP provision 字段（文档 vs 实际）
    st, raw, j = request("GET", f"{BASE_URL}/provision?space_id={sid}")
    if st == 200 and isinstance(j, dict):
        doc_fields = ["server", "port", "extension", "name", "auto_answer",
                      "center_ext", "multicast_addr", "ipc"]
        missing = [f for f in doc_fields if f not in j]
        extra = [f for f in j if f not in doc_fields]
        record("provision_shape", "GET /provision 字段与文档一致",
               "pass" if not missing else "fail",
               f"缺 {missing}, 多 {extra}", f"应含 {doc_fields}")
    else:
        record("provision_shape", "GET /provision", "fail",
               f"HTTP {st} {short(raw)}", "HTTP 200 + 文档字段")

    # 16. SIP 全部教室列表
    st, raw, j = request("GET", f"{BASE_URL}/provision/sip/endpoints")
    if st == 200 and isinstance(j, dict):
        has_center = "center" in j
        classrooms = (j.get("classrooms") or
                      (j.get("data") or {}).get("classrooms") or [])
        has_ext = bool(classrooms) and all("ext" in c for c in classrooms)
        enveloped = "data" in j and "status" in j
        record("endpoints_shape", "GET /provision/sip/endpoints 结构 {center, classrooms[].ext}",
               "pass" if (has_center and has_ext and not enveloped) else "fail",
               f"envelope={'data' in j} center={has_center} 教室数={len(classrooms)} 有ext={has_ext}",
               "裸 {center, classrooms[].ext}，无 envelope")
    else:
        record("endpoints_shape", "GET /provision/sip/endpoints", "fail",
               f"HTTP {st} {short(raw)}", "HTTP 200")

    # 17. 本教室摄像头
    st, raw, j = request("GET", f"{BASE_URL}/provision/ipc?space_id={sid}")
    if st == 200 and isinstance(j, dict):
        record("ipc_shape", "GET /provision/ipc 字段 {name, ext, ipc}",
               "pass" if ("ext" in j and "ipc" in j) else "fail",
               f"字段={sorted(j.keys())}", "应含 name/ext/ipc")
    else:
        record("ipc_shape", "GET /provision/ipc", "fail",
               f"HTTP {st} {short(raw)}", "HTTP 200 {name, ext, ipc}")

    # 18. face/login 错误路径
    st, raw, j = request("POST", f"{BASE_URL}/api/face/login",
                         body={"feature": [0.1] * 10, "space_id": sid})
    record("facelogin_dim", "POST /api/face/login 10 维特征返回 400",
           "pass" if st == 400 else "fail", f"HTTP {st} {short(raw)}", "HTTP 400")

    st, raw, j = request("POST", f"{BASE_URL}/api/face/login", body={"space_id": sid})
    record("facelogin_missing", "POST /api/face/login 缺 feature 返回 400",
           "pass" if st == 400 else "fail", f"HTTP {st} {short(raw)}", "HTTP 400")

    # 19. card/login 未知卡
    st, raw, j = request("POST", f"{BASE_URL}/api/card/login",
                         body={"rf_id": "DEADBEEF00", "space_id": sid})
    record("cardlogin_unknown", "POST /api/card/login 未知卡号返回 401",
           "pass" if st == 401 else "fail", f"HTTP {st} {short(raw)}", "HTTP 401")

    # 20. card/bind 错误密码
    st, raw, j = request("POST", f"{BASE_URL}/api/card/bind",
                         body={"username": "2502816", "password": "wrongpass", "rf_id": "0000000000"})
    record("cardbind_badpw", "POST /api/card/bind 错误密码返回 401",
           "pass" if st == 401 else "fail", f"HTTP {st} {short(raw)}", "HTTP 401")

    # 21. /chat 错误路径（服务可能宕机，不可达时跳过而非判定为接口错误）
    st, raw, j = request("POST", f"{CHAT_URL}/chat", token=token,
                         body={"space_id": sid, "text": ""})
    if st is None:
        record("chat_empty", "POST /chat 空 text 返回 400", "skip", f"服务不可达: {raw}")
    else:
        record("chat_empty", "POST /chat 空 text 返回 400",
               "pass" if st == 400 else "fail", f"HTTP {st} {short(raw)}", "HTTP 400")

    st, raw, j = request("POST", f"{CHAT_URL}/chat", body={"space_id": sid, "text": "开灯"})
    if st is None:
        record("chat_noauth", "POST /chat 无 token 返回 401", "skip", f"服务不可达: {raw}")
    else:
        record("chat_noauth", "POST /chat 无 token 返回 401",
               "pass" if st == 401 else "fail", f"HTTP {st} {short(raw)}", "HTTP 401")

    # 收集所有空间的设备（供变更性测试跨空间选取有效网关设备）
    all_devices = []
    for sp in space_ids:
        st, raw, j = request("GET", f"{BASE_URL}/api/devices?space_id={sp}", token=token)
        if st == 200 and isinstance(j, list):
            for d in j:
                if d.get("device_id") not in {x.get("device_id") for x in all_devices}:
                    all_devices.append(d)

    return {"token": token, "space_ids": space_ids, "sid": sid, "devices": devices,
            "all_devices": all_devices, "zig_id": zig_id}


# --------------------------------------------------------------------------
# WebSocket 检查
# --------------------------------------------------------------------------
async def _ws_checks(token, sid):
    if websockets is None:
        record("ws_lib", "websockets 库不可用，WebSocket 检查跳过", "skip")
        return

    base_ws = BASE_URL.replace("http://", "ws://").replace("https://", "wss://")

    # 正常连接 + 快照
    try:
        async with websockets.connect(f"{base_ws}/ws?token={token}&space_id={sid}",
                                      open_timeout=10) as ws:
            got = []
            try:
                while True:
                    m = await asyncio.wait_for(ws.recv(), timeout=6)
                    d = json.loads(m)
                    got.append(d)
                    if len(got) >= 4:
                        break
            except asyncio.TimeoutError:
                pass
        devs = [g for g in got if g.get("type") == "device_update"]
        ok = bool(devs) and all(
            all(k in g for k in ("device_id", "device_type", "status", "app", "space_id"))
            for g in devs)
        record("ws_snapshot", f"WebSocket 连接后收到 device_update 快照 ({len(devs)} 条) 且结构完整",
               "pass" if ok else "fail",
               f"共 {len(got)} 条消息, device_update={len(devs)}, 首条={short(devs[0] if devs else got)}",
               "device_update 含 device_id/device_type/status/app/space_id")
    except Exception as e:
        record("ws_snapshot", "WebSocket 正常连接", "fail", f"<exception: {e}>", "连接成功并收到快照")

    # 坏 token / 无 token 拒绝
    for label, url in [("坏 token", f"{base_ws}/ws?token=BAD&space_id={sid}"),
                       ("无 token", f"{base_ws}/ws?space_id={sid}")]:
        try:
            async with websockets.connect(url, open_timeout=8):
                record(f"ws_{label}", f"WebSocket {label} 应被拒绝",
                       "fail", "连接成功", "HTTP 401 拒绝")
        except Exception as e:
            rejected = "401" in str(e)
            record(f"ws_{label}", f"WebSocket {label} 被 401 拒绝",
                   "pass" if rejected else "fail", str(e), "HTTP 401")

    # 不带 space_id（管理后台，收全量）
    try:
        async with websockets.connect(f"{base_ws}/ws?token={token}", open_timeout=8) as ws:
            m = await asyncio.wait_for(ws.recv(), timeout=6)
            d = json.loads(m)
            record("ws_nospace", "WebSocket 不带 space_id 能连接（收全量）",
                   "pass", f"首条 type={d.get('type')}", "连接成功")
    except Exception as e:
        record("ws_nospace", "WebSocket 不带 space_id 能连接", "fail", str(e), "连接成功")

    # 30s ping 心跳
    if CHECK_PING:
        try:
            async with websockets.connect(f"{base_ws}/ws?token={token}&space_id={sid}",
                                          open_timeout=10) as ws:
                ping_at = None
                deadline = time.time() + 33
                while time.time() < deadline:
                    try:
                        m = await asyncio.wait_for(ws.recv(), timeout=min(5, deadline - time.time()))
                    except asyncio.TimeoutError:
                        continue
                    d = json.loads(m)
                    if d.get("type") == "ping":
                        ping_at = time.time()
                        break
                record("ws_ping", "WebSocket 30s 内收到 ping 心跳",
                       "pass" if ping_at else "fail", "收到 ping" if ping_at else "33s 内未收到", "type=ping")
        except Exception as e:
            record("ws_ping", "WebSocket ping 心跳", "fail", str(e), "type=ping")
    else:
        record("ws_ping", "WebSocket ping 心跳（CHECK_PING=0 跳过）", "skip")


# --------------------------------------------------------------------------
# 变更性测试
# --------------------------------------------------------------------------
async def mutating_checks(ctx):
    if not RUN_MUTATING:
        record("mutating", "变更性测试（RUN_MUTATING=0 跳过，不作为 FAIL/SKIP 计数）", "info")
        return
    token = ctx["token"]
    devices = ctx.get("all_devices") or ctx.get("devices") or []
    switches = [d for d in devices if d.get("device_type") == "zigbee"
                and d.get("zb_type") == "switch"]
    if not switches:
        record("mutating", "未找到 zigbee switch 设备，变更性测试跳过", "info")
        return

    base_ws = BASE_URL.replace("https://", "wss://").replace("http://", "ws://")

    async def drain(ws, secs):
        """收取 secs 秒内的 WS 消息，返回 list[dict]。"""
        out = []
        if ws is None:
            return out
        end = time.time() + secs
        while time.time() < end:
            try:
                m = await asyncio.wait_for(ws.recv(), timeout=max(1, end - time.time()))
            except Exception:
                break
            try:
                out.append(json.loads(m))
            except Exception:
                continue
        return out

    # P0-5 关键验收：gateway_id=gw-unassigned 的设备，控制必须被拒绝（400/404），不能 200 假成功
    unassigned = [d for d in switches if d.get("gateway_id") == "gw-unassigned"]
    if unassigned:
        d = unassigned[0]
        st, raw, j = request("POST", f"{BASE_URL}/api/devices/zigbee/{d['device_id']}/cmd",
                             token=token, body={"command": "toggle"})
        record("mut_gw_unassigned", f"对 gw-unassigned 设备 {d['device_id']} 控制应被拒绝",
               "pass" if st in (400, 404) else "fail",
               f"HTTP {st} {short(raw)}", "400（未绑定网关）或 404；不应 200 假成功")
    else:
        record("mut_gw_unassigned", "gw-unassigned 设备控制校验", "skip", "无 gw-unassigned 设备")

    # 有效网关设备：观察 200/topic 与回执（结果依赖网关在线与否，仅 INFO 记录）
    valid = [d for d in switches if d.get("gateway_id") not in ("gw-unassigned", "", None)]
    if valid:
        d = valid[0]
        ws = None
        if websockets:
            try:
                ws = await websockets.connect(f"{base_ws}/ws?token={token}", open_timeout=8)
            except Exception as e:
                record("mut_valid_gw", "有效网关设备控制校验", "info", f"WS 连接失败: {e}")
        st, raw, j = request("POST", f"{BASE_URL}/api/devices/zigbee/{d['device_id']}/cmd",
                             token=token, body={"command": "toggle"})
        topic = (j or {}).get("topic", "") if st == 200 else ""
        record("mut_valid_gw", f"对有效网关设备 {d['device_id']} 控制（观察）",
               "info", f"HTTP {st} {short(raw)}",
               "期望：设备在线时 200 + 真实 topic + cmd_response/device_update")
        ev = await drain(ws, 8)
        cr = [e for e in ev if e.get("type") == "cmd_response" and e.get("device_id") == d["device_id"]]
        du = [e for e in ev if e.get("type") == "device_update" and e.get("device_id") == d["device_id"]]
        record("mut_valid_echo", f"等待 cmd_response/device_update（{d['device_id']}）",
               "info", f"cmd_response={len(cr)} device_update={len(du)}（device_update 含初始快照）",
               "期望 cmd_response + 状态变化的 device_update（设备在线时）")
        if ws is not None:
            try:
                await ws.close()
            except Exception:
                pass
    else:
        record("mut_valid_gw", "有效网关设备控制校验", "skip", "无有效网关的 zigbee switch")

    # /chat 真实指令
    sid = ctx["sid"]
    st, raw, j = request("POST", f"{CHAT_URL}/chat", token=token,
                         body={"space_id": sid, "text": "把灯打开"})
    if st is None:
        record("mut_chat", f"POST /chat '把灯打开' (space={sid})", "info",
               f"服务不可达: {raw}")
    else:
        record("mut_chat", f"POST /chat '把灯打开' (space={sid})",
               "info", f"HTTP {st} {short(raw)}", "200 返回 reply/actions")


# --------------------------------------------------------------------------
def main():
    import asyncio
    ctx = read_only_checks()
    if ctx and ctx.get("token"):
        asyncio.run(_ws_checks(ctx["token"], ctx["sid"]))
        asyncio.run(mutating_checks(ctx))

    # 汇总
    n = {"pass": 0, "fail": 0, "skip": 0, "info": 0}
    for r in RESULTS:
        n[r["status"]] += 1
    print("\n" + "=" * 60)
    print(f"汇总: PASS={n['pass']} FAIL={n['fail']} SKIP={n['skip']} INFO={n['info']}")
    fails = [r for r in RESULTS if r["status"] == "fail"]
    if fails:
        print("\n发现问题（FAIL）：")
        for r in fails:
            print(f"  - {r['id']}: {r['name']}")
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "results.json")
    with open(out, "w", encoding="utf-8") as f:
        json.dump({"summary": n, "results": RESULTS,
                   "time": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())},
                  f, ensure_ascii=False, indent=2)
    print(f"\n结果已写入 {out}")
    return 1 if n["fail"] else 0


if __name__ == "__main__":
    sys.exit(main())
