#!/usr/bin/env python3
"""7.7 端到端验证脚本（对齐文档与代码 + 验收）。

跑通七段链路：登录绑定 → 角色同步 → 空间绑定 → 控制鉴权 → 告警推送 →
设备控制 → 空间同步。每段至少一条成功断言 + 一条失败/边界断言。

前置：
    python3 scripts/dev/mock-go-backend.py        # 监听 8081
    ./build/wechat-gateway                        # 监听 8080（dev：内存仓库 + mock 渠道）

说明：
  - login 的 jscode2session 走真实微信，离线不可测；bind 直接注入本地签发的
    openid_ticket 验证「go-backend 登录 + 绑定落库 + JWT 签发」。
  - 用共享 JWT 密钥本地签发 access token，模拟「旧 JWT（teacher）」与
    「重登后新 JWT（admin）」以演示角色分叉（7.7 收尾项 d）。
"""

import base64
import hashlib
import hmac
import json
import sys
import time
import urllib.error
import urllib.request

GATEWAY = "http://127.0.0.1:8080"
INTERNAL_TOKEN = "dev-internal-token"     # 与 config.json 默认一致
JWT_SECRET = "dev-shared-jwt-secret"       # 与 config.json 默认一致

# ---- JWT（与 src/utils/src/JwtUtil.cpp 的 HS256 base64url 一致） ----

def _b64url(b: bytes) -> str:
    return base64.urlsafe_b64encode(b).rstrip(b"=").decode()


def _jwt(payload: dict, secret: str) -> str:
    h = _b64url(json.dumps({"alg": "HS256", "typ": "JWT"},
                           separators=(",", ":")).encode())
    p = _b64url(json.dumps(payload, separators=(",", ":")).encode())
    sig = _b64url(hmac.new(secret.encode(), f"{h}.{p}".encode(),
                           hashlib.sha256).digest())
    return f"{h}.{p}.{sig}"


def openid_ticket(openid: str) -> str:
    return _jwt({"openid": openid, "token_type": "openid_ticket",
                 "exp": int(time.time()) + 300}, JWT_SECRET)


def access_token(user_id: str, role: str) -> str:
    return _jwt({"user_id": user_id, "role": role,
                 "exp": int(time.time()) + 7200}, JWT_SECRET)


# ---- HTTP 辅助 ----

PASS = 0
FAIL = 0


def req(method, path, headers=None, body=None):
    url = GATEWAY + path
    data = json.dumps(body, ensure_ascii=False).encode() if body is not None else None
    r = urllib.request.Request(url, data=data, method=method, headers=headers or {})
    try:
        with urllib.request.urlopen(r, timeout=15) as resp:
            return resp.status, resp.read().decode("utf-8", "replace")
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode("utf-8", "replace")
    except Exception as e:  # noqa: BLE001
        return None, f"<error: {e}>"


def check(name, cond, detail):
    global PASS, FAIL
    mark = "PASS" if cond else "FAIL"
    print(f"[{mark}] {name}\n       {detail}")
    if cond:
        PASS += 1
    else:
        FAIL += 1


def auth(token):
    return {"Authorization": f"Bearer {token}"}


def internal():
    return {"X-Internal-Token": INTERNAL_TOKEN, "Content-Type": "application/json"}


def main():
    print("=" * 72)
    print("7.7 端到端验证")
    print("=" * 72)

    # ---- 段 1：登录绑定 ----
    print("\n--- 段 1 登录绑定 ---")
    ticket = openid_ticket("test_openid_admin")
    s, b = req("POST", "/api/miniapp/bind",
               headers={"Content-Type": "application/json"},
               body={"openid_token": ticket, "username": "admin",
                     "password": "secret"})
    print(f"  curl -X POST {GATEWAY}/api/miniapp/bind "
          f"-d '{{\"openid_token\":\"<crafted>\",\"username\":\"admin\",\"password\":\"secret\"}}'")
    print(f"  -> HTTP {s} {b}")
    admin_jwt = None
    if s == 200:
        admin_jwt = json.loads(b).get("token")
    check("bind 成功返回 access token（admin/u_admin_1）", s == 200 and admin_jwt,
          f"HTTP {s}, token={'<set>' if admin_jwt else '<missing>'}")

    s, b = req("POST", "/api/miniapp/bind",
               headers={"Content-Type": "application/json"},
               body={"openid_token": "garbage.token.value", "username": "admin",
                     "password": "secret"})
    print(f"  -> HTTP {s} {b}")
    check("bind 非法 openid_token 被拒（401）", s == 401,
          f"HTTP {s}")

    # ---- 段 2：角色同步（幂等 + 实时角色覆盖） ----
    print("\n--- 段 2 角色同步（幂等 + 实时角色覆盖） ---")
    # 用独立演示用户 u_role_demo，避免把 u_teacher_1 改成 admin 后污染其 60s 角色
    # 缓存——u_teacher_1 后续段仍作 teacher 使用（启动种子已绑定 spc_a8acdd5c）。
    body = {"user_id": "u_role_demo", "roles": ["admin"]}
    s, b = req("POST", "/internal/user-roles/sync", headers=internal(), body=body)
    print(f"  curl -X POST {GATEWAY}/internal/user-roles/sync -H 'X-Internal-Token: ...' "
          f"-d '{json.dumps(body)}'")
    print(f"  -> HTTP {s} {b}")
    check("角色同步成功（200 ok）", s == 200, f"HTTP {s} {b}")
    s2, _ = req("POST", "/internal/user-roles/sync", headers=internal(), body=body)
    check("重复同步幂等（再次 200）", s2 == 200, f"HTTP {s2}")

    s, b = req("POST", "/internal/user-roles/sync",
               headers={"Content-Type": "application/json"}, body=body)
    print(f"  -> 无 X-Internal-Token: HTTP {s} {b}")
    check("无内部令牌被拒（401）", s == 401, f"HTTP {s}")

    # 实时角色覆盖：同一 user_id，同步前铸造的旧 JWT（teacher）在同步后也立即按
    # user_roles 的新角色 admin 生效（无需重新登录）；重登的新 JWT（admin）结果一致。
    old_jwt = access_token("u_role_demo", "teacher")
    new_jwt = access_token("u_role_demo", "admin")
    s, b = req("GET", "/api/miniapp/spaces", headers=auth(old_jwt))
    print(f"  -> 旧 JWT(teacher) GET /spaces: HTTP {s} {b}")
    s2, b2 = req("GET", "/api/miniapp/spaces", headers=auth(new_jwt))
    print(f"  -> 新 JWT(admin)   GET /spaces: HTTP {s2} {b2}")
    old_ids = [x["space_id"] for x in json.loads(b).get("spaces", [])] if s == 200 else []
    new_ids = [x["space_id"] for x in json.loads(b2).get("spaces", [])] if s2 == 200 else []
    check("角色同步后旧 JWT 实时按 admin 生效（无需重登）",
          "spc_a8acdd5c" in old_ids and "spc_b7bee44d" in old_ids,
          f"old={old_ids}")
    check("重登后新 JWT 同样按 admin 生效（全部空间）",
          "spc_a8acdd5c" in new_ids and "spc_b7bee44d" in new_ids,
          f"new={new_ids}")

    # 后续空间绑定/控制段的教师身份：u_teacher_1 启动种子即为 teacher（绑定
    # spc_a8acdd5c），本段未改动其角色，直接铸造 teacher JWT 复用。
    teacher_jwt = access_token("u_teacher_1", "teacher")

    # ---- 段 3：空间绑定 ----
    print("\n--- 段 3 空间绑定 ---")
    s, b = req("POST", "/api/miniapp/space-bindings", headers=auth(admin_jwt),
               body={"user_id": "u_teacher_1", "space_id": "spc_std_a102"})
    print(f"  -> admin 绑定 u_teacher_1->spc_std_a102: HTTP {s} {b}")
    check("admin 绑定成功（200）", s == 200, f"HTTP {s} {b}")

    s, b = req("POST", "/api/miniapp/space-bindings", headers=auth(teacher_jwt),
               body={"user_id": "u_teacher_1", "space_id": "spc_b7bee44d"})
    print(f"  -> teacher 越权绑定: HTTP {s} {b}")
    check("teacher 绑定被拒（403）", s == 403, f"HTTP {s}")

    # ---- 段 4：控制鉴权 ----
    print("\n--- 段 4 控制鉴权 ---")
    s, b = req("POST", "/api/miniapp/device/control", headers=auth(teacher_jwt),
               body={"space_id": "spc_a8acdd5c", "device_type": "fuhe-screen",
                     "device_id": "dev_c2936745", "command": "on"})
    print(f"  -> teacher 控制本教室(spc_a8acdd5c): HTTP {s} {b}")
    check("teacher 控制已绑定空间成功（200）", s == 200, f"HTTP {s}")

    s, b = req("POST", "/api/miniapp/device/control", headers=auth(teacher_jwt),
               body={"space_id": "spc_b7bee44d", "device_type": "fuhe-screen",
                     "device_id": "dev_c2936745", "command": "on"})
    print(f"  -> teacher 越权控制(spc_b7bee44d): HTTP {s} {b}")
    check("teacher 越权控制被拒（403）", s == 403, f"HTTP {s}")

    # 门禁 428：先由 admin 标记门禁，再 off 无 confirm -> 428。
    s, b = req("POST", "/api/miniapp/door-devices", headers=auth(admin_jwt),
               body={"space_id": "spc_a8acdd5c", "device_id": "dev_1a2b3c4d",
                     "label": "前门"})
    print(f"  -> admin 标记门禁: HTTP {s} {b}")
    s, b = req("POST", "/api/miniapp/device/control", headers=auth(admin_jwt),
               body={"space_id": "spc_a8acdd5c", "device_type": "zigbee",
                     "device_id": "dev_1a2b3c4d", "command": "off"})
    print(f"  -> 门禁 off 无 confirm: HTTP {s} {b}")
    check("门禁 off 无 confirm 返回 428", s == 428, f"HTTP {s} {b}")
    s, b = req("POST", "/api/miniapp/device/control", headers=auth(admin_jwt),
               body={"space_id": "spc_a8acdd5c", "device_type": "zigbee",
                     "device_id": "dev_1a2b3c4d", "command": "off",
                     "confirm": True})
    print(f"  -> 门禁 off 带 confirm: HTTP {s} {b}")
    check("门禁 off 带 confirm 通过（200）", s == 200, f"HTTP {s}")

    # ---- 段 5：告警推送 ----
    print("\n--- 段 5 告警推送 ---")
    ev = {"event_id": "evt_offline_1", "event_type": "device_offline",
          "space_id": "spc_a8acdd5c", "device_id": "dev_screen_01",
          "device_label": "综合屏", "severity": "warning",
          "content": "A101 综合屏已离线", "transition": "online->offline",
          "target": {"roles": ["admin"], "space_teachers": True}}
    s, b = req("POST", "/internal/notify/send", headers=internal(), body=ev)
    print(f"  -> device_offline: HTTP {s} {b}")
    st1 = json.loads(b).get("final_status") if s == 200 else None
    check("可推送事件被处理（success/partial）",
          s == 200 and st1 in ("success", "partial"), f"HTTP {s} status={st1}")

    ev2 = dict(ev, event_id="evt_offline_2")
    s, b = req("POST", "/internal/notify/send", headers=internal(), body=ev2)
    st2 = json.loads(b).get("final_status") if s == 200 else None
    print(f"  -> 同 device 冷却期内重发: HTTP {s} status={st2}")
    check("冷却期内重发被抑制（skipped）", s == 200 and st2 == "skipped",
          f"HTTP {s} status={st2}")

    ev3 = {"event_id": "evt_unknown_1", "event_type": "face_login_success",
           "space_id": "spc_a8acdd5c", "content": "张三登录成功",
           "target": {"roles": ["admin"]}}
    s, b = req("POST", "/internal/notify/send", headers=internal(), body=ev3)
    st3 = json.loads(b).get("final_status") if s == 200 else None
    print(f"  -> 不可推送类型 face_login_success: HTTP {s} status={st3}")
    check("不可推送类型被跳过（skipped）", s == 200 and st3 == "skipped",
          f"HTTP {s} status={st3}")

    # ---- 段 6：设备控制（端到端成功 + 命令白名单） ----
    print("\n--- 段 6 设备控制 ---")
    s, b = req("POST", "/api/miniapp/device/control", headers=auth(admin_jwt),
               body={"space_id": "spc_a8acdd5c", "device_type": "fuhe-screen",
                     "device_id": "dev_c2936745", "command": "toggle"})
    print(f"  -> admin toggle 综合屏: HTTP {s} {b}")
    check("admin 控制成功并透传 mock 回执（200）", s == 200, f"HTTP {s} {b}")

    s, b = req("POST", "/api/miniapp/device/control", headers=auth(admin_jwt),
               body={"space_id": "spc_a8acdd5c", "device_type": "fuhe-screen",
                     "device_id": "dev_c2936745", "command": "restart"})
    print(f"  -> admin 非法 command restart: HTTP {s} {b}")
    check("非法 command 被拒（400）", s == 400, f"HTTP {s} {b}")

    # ---- 段 7：空间同步（全量对账 + 级联清理） ----
    print("\n--- 段 7 空间同步（全量对账 + 级联清理） ---")
    # 先在被删空间埋一枚门禁标记，用于验证 door_devices 级联清理。
    s, b = req("POST", "/api/miniapp/door-devices", headers=auth(admin_jwt),
               body={"space_id": "spc_b7bee44d", "device_id": "dev_door_b7b",
                     "label": "报告厅门"})
    print(f"  -> 预埋 spc_b7bee44d 门禁: HTTP {s} {b}")

    s, b = req("POST", "/internal/spaces/sync", headers=internal(),
               body={"spaces": [
                   {"space_id": "spc_a8acdd5c", "name": "A101 教室", "type": "standard"},
                   {"space_id": "spc_std_a102", "name": "A102 教室", "type": "standard"},
               ]})
    print(f"  -> 全量同步（省略 spc_b7bee44d）: HTTP {s} {b}")
    removed = json.loads(b).get("removed") if s == 200 else None
    check("空间同步成功且移除 1 个缺失空间", s == 200 and removed == 1,
          f"HTTP {s} {b}")

    s, b = req("GET", "/api/miniapp/spaces", headers=auth(new_jwt))
    ids = [x["space_id"] for x in json.loads(b).get("spaces", [])] if s == 200 else []
    print(f"  -> 对账后 GET /spaces: HTTP {s} {b}")
    check("被删空间已消失（spc_b7bee44d 不在）",
          s == 200 and "spc_b7bee44d" not in ids, f"ids={ids}")

    # 级联清理断言 1：door_devices（被删空间的预埋门禁应被清掉）。
    s, b = req("GET", "/api/miniapp/door-devices?space_id=spc_b7bee44d",
               headers=auth(admin_jwt))
    doors = json.loads(b).get("doors", []) if s == 200 else None
    print(f"  -> 对账后 GET /door-devices?space_id=spc_b7bee44d: HTTP {s} {b}")
    check("door_devices 级联清理（被删空间门禁为空）",
          s == 200 and doors == [], f"doors={doors}")

    # 级联清理断言 2：user_spaces（u_teacher_2 原绑定 spc_b7bee44d，应被清空）。
    t2 = access_token("u_teacher_2", "teacher")
    s, b = req("GET", "/api/miniapp/spaces", headers=auth(t2))
    t2_ids = [x["space_id"] for x in json.loads(b).get("spaces", [])] if s == 200 else None
    print(f"  -> 对账后 u_teacher_2 GET /spaces: HTTP {s} {b}")
    check("user_spaces 级联清理（u_teacher_2 绑定被清空）",
          s == 200 and t2_ids == [], f"ids={t2_ids}")

    s, b = req("POST", "/internal/spaces/sync",
               headers={"Content-Type": "application/json"},
               body={"spaces": []})
    print(f"  -> 无内部令牌: HTTP {s} {b}")
    check("空间同步无内部令牌被拒（401）", s == 401, f"HTTP {s}")

    # ---- 汇总 ----
    print("\n" + "=" * 72)
    print(f"结果：{PASS} 通过 / {FAIL} 失败")
    print("=" * 72)
    return 0 if FAIL == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
