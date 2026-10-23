#!/usr/bin/env python3
"""AI 助手调试页本地服务器 + /api/* 反向代理（同源，免 CORS）。

用法：
    python3 tools/ai-debug/serve.py [port]
    然后 Windows 浏览器打开 http://localhost:<port>

网关地址默认 http://127.0.0.1:8080，可用环境变量 AI_GATEWAY 覆盖。
"""
import http.server
import os
import socketserver
import sys
import urllib.error
import urllib.request

GATEWAY = os.environ.get("AI_GATEWAY", "http://127.0.0.1:8080").rstrip("/")
PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
HERE = os.path.dirname(os.path.abspath(__file__))


class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path.split("?", 1)[0] in ("/", "/index.html"):
            self._serve_index()
        elif self.path.startswith("/api/"):
            self._proxy()
        else:
            self.send_error(404)

    def do_POST(self):
        if self.path.startswith("/api/"):
            self._proxy()
        else:
            self.send_error(404)

    def do_OPTIONS(self):
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Headers", "Content-Type, Authorization")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.end_headers()

    def _serve_index(self):
        with open(os.path.join(HERE, "index.html"), "rb") as f:
            data = f.read()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def _proxy(self):
        length = int(self.headers.get("Content-Length", 0) or 0)
        body = self.rfile.read(length) if length else b""
        req = urllib.request.Request(GATEWAY + self.path, data=body,
                                     method=self.command)
        if self.command == "POST":
            req.add_header("Content-Type",
                           self.headers.get("Content-Type", "application/json"))
        auth = self.headers.get("Authorization", "")
        if auth:
            req.add_header("Authorization", auth)
        try:
            with urllib.request.urlopen(req, timeout=180) as resp:
                self._reply(resp.status, resp.read())
        except urllib.error.HTTPError as e:
            self._reply(e.code, e.read())
        except Exception as e:
            self._reply(502, ('{"error":"gateway unreachable: %s"}' % e).encode())

    def _reply(self, status, data):
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def log_message(self, *args):
        pass


if __name__ == "__main__":
    print(f"[ai-debug] http://localhost:{PORT}  ->  proxy {GATEWAY}", flush=True)
    with socketserver.ThreadingTCPServer(("0.0.0.0", PORT), Handler) as httpd:
        httpd.serve_forever()
