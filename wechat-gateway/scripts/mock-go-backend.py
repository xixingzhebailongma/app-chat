#!/usr/bin/env python3
"""go-backend 服务的本地替身，用于离线开发/测试。

精确实现 C++ 网关所调用的接口（见 src/clients/
GoBackendClient.cpp），从而无需真实 go-backend 即可端到端跑通
网关的转发路径：

    POST /api/auth/login                      -> GoBackendClient::login
    GET  /devices?space_id=...                -> DeviceService::listDevices
    POST /devices/{type}/{id}/cmd             -> DeviceControlService::control

运行：python3 scripts/mock-go-backend.py   （监听 127.0.0.1:8081）
然后以 GO_BACKEND_URL=http://127.0.0.1:8081（默认）启动网关。

每个请求都会记录到 stdout（方法、路径、query、Authorization 头、
body），以便确认网关实际转发了什么。
"""

import json
import os
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

HOST, PORT = "127.0.0.1", 8081

# MOCK_NO_ZB_ROLE=1 时设备列表不输出 zb_role，用于回归「真实边侧无 zb_role」的
# label 兜底匹配路径（场景开灯/关灯靠 label 关键字，而非 zb_role）。
NO_ZB_ROLE = os.environ.get("MOCK_NO_ZB_ROLE") == "1"

# 演示账号库（代替真实 go-backend 的用户数据库）。
# username -> (password, user_id, role, name, phone)
# 空间归属记录在网关的 user_spaces seed 中（见 main.cpp）：
#   u_admin_1   -> 所有空间
#   u_teacher_1 -> spc_a8acdd5c (A101 教室)
#   u_teacher_2 -> spc_b7bee44d (B202 实验室)
ACCOUNTS = {
    "admin": ("admin123", "u_admin_1", "admin", "管理员", "13800000001"),
    "li": ("123456", "u_teacher_1", "teacher", "李老师", "13800000002"),
    "wang": ("123456", "u_teacher_2", "teacher", "王老师", "13800000003"),
}


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):  # 屏蔽默认的访问日志
        pass

    # ---- 请求处理管道 -----------------------------------------------------

    def _read_body(self):
        n = int(self.headers.get("Content-Length", 0) or 0)
        return self.rfile.read(n) if n else b""

    def _log_request(self, body: bytes):
        auth = self.headers.get("Authorization", "-")
        print(
            f"[mock-go-backend] {self.command} {self.path}\n"
            f"    Authorization: {auth}\n"
            f"    Body: {body.decode('utf-8', 'replace') or '-'}",
            flush=True,
        )

    def _json(self, status: int, payload: dict):
        data = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    # ---- 路由 -------------------------------------------------------------

    def do_POST(self):
        body = self._read_body()
        self._log_request(body)
        path = self.path.split("?", 1)[0]

        if path == "/api/auth/login":
            self._login(body)
        elif path.startswith("/api/devices/") and path.endswith("/cmd"):
            # /devices/{type}/{id}/cmd
            self._device_cmd(path, body)
        else:
            self._json(404, {"error": f"no such mock route: {path}"})

    def do_GET(self):
        body = self._read_body()
        self._log_request(body)
        path = self.path.split("?", 1)[0]

        if path == "/api/devices":
            self._devices()
        elif path.startswith("/api/users/"):
            # 短信兜底的手机号实时查询（GoBackendClient::getUserPhoneSync）
            self._user_phone(path.rstrip("/").split("/")[-1])
        else:
            self._json(404, {"error": f"no such mock route: {path}"})

    # ---- 处理器 -----------------------------------------------------------

    def _login(self, body: bytes):
        try:
            creds = json.loads(body or b"{}")
        except json.JSONDecodeError:
            self._json(400, {"message": "invalid JSON body"})
            return
        username = creds.get("username", "")
        password = creds.get("password", "")

        account = ACCOUNTS.get(username)
        if not account or account[0] != password:
            self._json(401, {"message": "invalid credentials"})
            return

        _, user_id, role, name, _phone = account
        # 综合屏 API 契约：登录返回 { token, user: { user_id, username, name, role } }。
        # 真实边侧 user_id 为 int；此处演示数据用字符串，网关已兼容两种类型（统一转字符串）。
        self._json(200, {
            "token": "mock-edge-token-" + user_id,
            "user": {
                "user_id": user_id,
                "username": username,
                "name": name,
                "role": role,
            },
        })

    def _user_phone(self, user_id: str):
        for _pw, uid, _role, _name, phone in ACCOUNTS.values():
            if uid == user_id:
                self._json(200, {"user_id": user_id, "phone": phone})
                return
        self._json(404, {"error": "user not found"})

    def _devices(self):
        query = parse_qs(urlparse(self.path).query)
        space_id = (query.get("space_id") or [""])[0]

        # 按空间返回设备；设备类型按空间类型各有所属
        # （标准教室：照明/电子班牌/综合屏/门禁；报告厅：主屏/照明/门禁/传感器；办公室：照明/门禁/传感器）。
        devices = {
            "spc_a8acdd5c": [  # A101 标准教室
                {"device_id": "dev_c2936745", "label": "教室照明", "type": "light", "online": True},
                {"device_id": "dev_1a2b3c4d", "label": "电子班牌", "type": "sign", "online": True},
                {"device_id": "dev_9f8e7d6c", "label": "综合屏", "type": "screen", "online": True},
                {"device_id": "dev_d0or_a101", "label": "前门门禁", "type": "door", "online": True},
                {"device_id": "dev_sens_a101", "label": "环境传感器", "type": "sensor", "online": True},
            ],
            "spc_std_a102": [  # A102 标准教室
                {"device_id": "dev_aa11bb22", "label": "教室照明", "type": "light", "online": True},
                {"device_id": "dev_cc33dd44", "label": "电子班牌", "type": "sign", "online": False},
                {"device_id": "dev_ee55ff66", "label": "综合屏", "type": "screen", "online": True},
                {"device_id": "dev_d0or_a102", "label": "前门门禁", "type": "door", "online": True},
            ],
            "spc_b7bee44d": [  # 多媒体报告厅
                {"device_id": "dev_55aa66bb", "label": "报告厅主屏", "type": "screen", "online": True},
                {"device_id": "dev_77cc88dd", "label": "报告厅照明", "type": "light", "online": True},
                {"device_id": "dev_99aabbcc", "label": "后门门禁", "type": "door", "online": False},
                {"device_id": "dev_sens_lect", "label": "环境传感器", "type": "sensor", "online": True},
            ],
            "spc_office_1": [  # 教师办公室
                {"device_id": "dev_off_light", "label": "办公区照明", "type": "light", "online": True},
                {"device_id": "dev_off_door", "label": "办公室门禁", "type": "door", "online": True},
                {"device_id": "dev_off_sens", "label": "温湿度传感器", "type": "sensor", "online": False},
            ],
            "spc_gym_1": [  # 体育馆
                {"device_id": "dev_gym_light", "label": "场馆照明", "type": "light", "online": True},
                {"device_id": "dev_gym_screen", "label": "大屏", "type": "screen", "online": True},
                {"device_id": "dev_gym_door", "label": "入口门禁", "type": "door", "online": True},
                {"device_id": "dev_gym_sens", "label": "环境传感器", "type": "sensor", "online": False},
            ],
            "spc_lab_1": [  # 化学实验室
                {"device_id": "dev_lab_light", "label": "实验区照明", "type": "light", "online": True},
                {"device_id": "dev_lab_screen", "label": "实验演示屏", "type": "screen", "online": True},
                {"device_id": "dev_lab_door", "label": "实验室门禁", "type": "door", "online": True},
                {"device_id": "dev_lab_sens", "label": "通风传感器", "type": "sensor", "online": False},
            ],
            "spc_lib_1": [  # 图书馆
                {"device_id": "dev_lib_light", "label": "阅览区照明", "type": "light", "online": True},
                {"device_id": "dev_lib_door", "label": "图书馆门禁", "type": "door", "online": True},
                {"device_id": "dev_lib_sens", "label": "环境传感器", "type": "sensor", "online": True},
            ],
            "spc_canteen_1": [  # 学生食堂
                {"device_id": "dev_ctn_light", "label": "餐厅照明", "type": "light", "online": True},
                {"device_id": "dev_ctn_screen", "label": "窗口大屏", "type": "screen", "online": True},
                {"device_id": "dev_ctn_door", "label": "入口门禁", "type": "door", "online": True},
                {"device_id": "dev_ctn_sens", "label": "温湿度传感器", "type": "sensor", "online": False},
            ],
        }
        # 转换成「综合屏 API 对接文档」的字段模型：type -> device_type(+zb_type)、
        # online(bool) -> status("online"/"offline")、开关 app="on"/"off"、传感器 app 为 JSON。
        def to_doc(dev):
            online = dev.get("online", True)
            t = dev["type"]
            d = {
                "device_id": dev["device_id"],
                "device_type": t,
                "status": "online" if online else "offline",
                "app": "on" if online else "off",
                "label": dev["label"],
                "space_id": space_id,
                "last_seen": "2026-09-11T04:52:49Z",
            }
            if t == "sensor":
                d["device_type"] = "zigbee"
                d["zb_type"] = "sensor"
                d["gateway_id"] = "dev_gw_" + space_id
                d["app"] = json.dumps(
                    {"temperature": 25.1, "humidity": 60, "co2": 800}
                )
            elif t in ("light", "door"):
                d["device_type"] = "zigbee"
                d["zb_type"] = "switch"
                # zb_role 仅用于 UI 展示；场景匹配在后端按 label 兜底，前端不参与场景匹配逻辑。
                # MOCK_NO_ZB_ROLE=1 时不输出，模拟真实边侧（无 zb_role）。
                if not NO_ZB_ROLE:
                    d["zb_role"] = t
                d["gateway_id"] = "dev_gw_" + space_id
            elif t == "sign":
                d["device_type"] = "fuhe-board"
                d["info"] = "{}"
            elif t == "screen":
                d["device_type"] = "fuhe-screen"
                d["info"] = "{}"
            return d

        self._json(200, [to_doc(d) for d in devices.get(space_id, [])])

    def _device_cmd(self, path: str, body: bytes):
        # path = /api/devices/{type}/{id}/cmd
        parts = path.strip("/").split("/")
        device_type, device_id = parts[2], parts[3]
        self._json(200, {
            "ok": True,
            "device_type": device_type,
            "device_id": device_id,
            "command": json.loads(body or b"{}").get("command", ""),
            "echo": "mock-go-backend received your command",
        })


def main():
    server = ThreadingHTTPServer((HOST, PORT), Handler)
    print(f"[mock-go-backend] listening on http://{HOST}:{PORT}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[mock-go-backend] stopped", flush=True)
        sys.exit(0)


if __name__ == "__main__":
    main()
