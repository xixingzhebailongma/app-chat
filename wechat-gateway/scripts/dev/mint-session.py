#!/usr/bin/env python3
"""为本地 H5 联调签一张会话 JWT，并打印「粘贴到浏览器 DevTools console」的注入一行式。

用途：小程序真实模式登录要 wx.login（H5 浏览器拿不到微信 code），本地联调时用
本脚本直接签一张与网关共享 secret 的 JWT，注入 uni-app H5 的 session 存储，跳过登录。

用法：python3 scripts/dev/mint-session.py
然后把输出里对应角色的一行，粘贴到 http://localhost:5173 的浏览器控制台回车。
"""

import base64
import hashlib
import hmac
import json
import time

SECRET = "dev-shared-jwt-secret"
TTL = 259200  # 3 天，与网关 access_ttl_seconds 一致


def b64url(b: bytes) -> str:
    return base64.urlsafe_b64encode(b).rstrip(b"=").decode()


def mint(user_id: str, role: str) -> str:
    header = b64url(json.dumps({"alg": "HS256", "typ": "JWT"}, separators=(",", ":")).encode())
    payload = b64url(
        json.dumps(
            {"user_id": user_id, "role": role, "exp": int(time.time()) + TTL},
            separators=(",", ":"),
        ).encode()
    )
    signing = (header + "." + payload).encode()
    sig = b64url(hmac.new(SECRET.encode(), signing, hashlib.sha256).digest())
    return header + "." + payload + "." + sig


def inject_line(token: str, user_id: str, role: str, name: str) -> str:
    session = {"token": token, "userId": user_id, "role": role, "name": name}
    # uni-app H5 存储编码：对象 -> {"type":"object","data":{...}}
    stored = json.dumps({"type": "object", "data": session}, ensure_ascii=False)
    return (
        "localStorage.setItem('session', "
        + json.dumps(stored, ensure_ascii=False)
        + "); location.reload();"
    )


def main():
    admin = mint("u_admin_1", "admin")
    teacher = mint("u_teacher_1", "teacher")

    print("=== 管理员（admin / u_admin_1）===")
    print(inject_line(admin, "u_admin_1", "admin", "管理员"))
    print()
    print("=== 教师（李老师 / u_teacher_1，绑定 A101 教室）===")
    print(inject_line(teacher, "u_teacher_1", "teacher", "李老师"))
    print()
    print("用法：在 http://localhost:5173 打开 DevTools console，粘贴上面任一行回车。")


if __name__ == "__main__":
    main()
