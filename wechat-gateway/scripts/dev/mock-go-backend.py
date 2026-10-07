#!/usr/bin/env python3
"""go-backend 服务的本地替身，用于离线开发/测试。

精确实现 C++ 网关所调用的接口（见 src/clients/
GoBackendClient.cpp），从而无需真实 go-backend 即可端到端跑通
网关的转发路径：

    POST /api/auth/login              -> GoBackendClient::login
    GET  /devices?space_id=...        -> DeviceService::listDevices
    POST /devices/{type}/{id}/cmd     -> DeviceControlService::control

运行：python3 scripts/dev/mock-go-backend.py   （监听 127.0.0.1:8081）
然后在 8080 端口启动网关（默认）；GO_BACKEND_URL 默认为
http://127.0.0.1:8081。

每个请求都会记录到 stdout（方法、路径、Authorization 头、body），以便
确认网关实际转发了什么。
"""

import json
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HOST, PORT = "127.0.0.1", 8081

# 演示手机号（短信兜底实时查询 GET /api/users/{user_id} 返回）。
USER_PHONES = {
    "u_admin_1": "13800000001",
    "u_teacher_1": "13800000002",
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

        if not password:
            self._json(401, {"message": "invalid credentials"})
            return

        # 演示账号："admin" 解析为管理员；其它账号都解析为教师。
        # 响应遵循 GoBackendClient::login 契约：{"user": {...}}，网关从
        # user 对象里取 user_id/username/name/role。
        if username == "admin":
            self._json(200, {"user": {"user_id": "u_admin_1",
                                      "username": "admin",
                                      "name": "管理员", "role": "admin"}})
        else:
            self._json(200, {"user": {"user_id": "u_teacher_1",
                                      "username": username,
                                      "name": "李老师", "role": "teacher"}})

    def _devices(self):
        # 返回裸设备数组（综合屏 API 约定）；网关 DeviceService 会把它包装
        # 成 {"devices":[...]}。字段用简化模型即可，网关只做代理不校验。
        self._json(200, [
            {"device_id": "dev_c2936745", "label": "综合屏",
             "device_type": "fuhe-screen", "status": "online"},
            {"device_id": "dev_1a2b3c4d", "label": "门禁",
             "device_type": "zigbee", "status": "online"},
        ])

    def _user_phone(self, user_id: str):
        phone = USER_PHONES.get(user_id)
        if phone is None:
            self._json(404, {"error": "user not found"})
            return
        self._json(200, {"user_id": user_id, "phone": phone})

    def _device_cmd(self, path: str, body: bytes):
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
