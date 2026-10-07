#!/usr/bin/env python3
"""边侧进出记录模拟器 —— 演示/联调用，向网关 `POST /internal/access-records`
灌入人脸/刷卡进出记录，把「边侧 face_login/card_login → 网关 ingest →
小程序今日进出」这条链路跑通、可演示。

注意：仅用于演示/联调，正式验收必须等真实边侧数据流入
（见 docs/边侧数据源确认.md 的「进出记录接入契约」）。

运行（先启动网关 ./build/wechat-gateway，监听 8080）：
    python3 scripts/dev/access-record-simulator.py --once
    python3 scripts/dev/access-record-simulator.py --loop --interval 30

幂等：event_id 用 evt_face_<time_ns> 保证唯一；重发同 event_id 网关
ON CONFLICT DO NOTHING，仍返回 200 但不重复落库。
"""

import argparse
import json
import os
import sys
import time
import urllib.error
import urllib.request

DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 8080
DEFAULT_TOKEN = "dev-internal-token"

SPACE_ID = "spc_a8acdd5c"


def sample_records() -> list:
    """三条样例：李老师 face login、王老师 card login、陌生人 face denied。"""
    return [
        {"name": "李老师", "auth_type": "face", "result": "login"},
        {"name": "王老师", "auth_type": "card", "result": "login"},
        {"name": "陌生人", "auth_type": "face", "result": "denied"},
    ]


def build_payload(rec: dict, occurred_at: str) -> dict:
    """按 AccessRecordService::ingest 的契约组装请求体（occurred_at 为 ISO8601）。"""
    return {
        "event_id": f"evt_face_{time.time_ns()}",
        "space_id": SPACE_ID,
        "occurred_at": occurred_at,
        "name": rec["name"],
        "auth_type": rec["auth_type"],
        "result": rec["result"],
        "device_id": "dev_face_01",
    }


def send_record(host: str, port: int, token: str, payload: dict) -> bool:
    """向网关 /internal/access-records 发送单条记录，返回是否成功送达。"""
    url = f"http://{host}:{port}/internal/access-records"
    data = json.dumps(payload, ensure_ascii=False).encode("utf-8")
    req = urllib.request.Request(
        url,
        data=data,
        method="POST",
        headers={
            "Content-Type": "application/json; charset=utf-8",
            "X-Internal-Token": token,
        },
    )
    try:
        with urllib.request.urlopen(req, timeout=5) as resp:
            status, body = resp.status, resp.read().decode("utf-8", "replace")
    except urllib.error.HTTPError as e:
        status, body = e.code, e.read().decode("utf-8", "replace")
    except urllib.error.URLError as e:
        print(
            f"[access-record-simulator] 网关不可达 {url}: {e.reason}",
            file=sys.stderr,
            flush=True,
        )
        return False

    print(
        f"[access-record-simulator] {payload['name']:<6} {payload['result']:<6}"
        f" -> HTTP {status} {body.strip()}",
        flush=True,
    )
    return True


def fire_round(host: str, port: int, token: str) -> None:
    occurred_at = time.strftime("%Y-%m-%dT%H:%M:%S+08:00", time.localtime())
    for rec in sample_records():
        send_record(host, port, token, build_payload(rec, occurred_at))


def main() -> None:
    parser = argparse.ArgumentParser(
        description="边侧进出记录模拟器（演示/联调）"
    )
    parser.add_argument("--host", default=DEFAULT_HOST, help="网关地址")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="网关端口")
    parser.add_argument(
        "--token",
        default=os.environ.get("INTERNAL_TOKEN", DEFAULT_TOKEN),
        help="内部令牌（默认 dev-internal-token，可用 INTERNAL_TOKEN 覆盖）",
    )
    parser.add_argument(
        "--once", action="store_true", help="一次性发完记录后退出（默认行为）"
    )
    parser.add_argument(
        "--loop", action="store_true", help="持续按 --interval 循环发送"
    )
    parser.add_argument(
        "--interval", type=float, default=30.0, help="--loop 模式下每轮间隔秒数"
    )
    args = parser.parse_args()

    if args.loop:
        print(
            f"[access-record-simulator] 持续模式：每 {args.interval}s 发一轮"
            "（Ctrl+C 停止）",
            flush=True,
        )
        try:
            while True:
                fire_round(args.host, args.port, args.token)
                time.sleep(args.interval)
        except KeyboardInterrupt:
            print("\n[access-record-simulator] 已停止", flush=True)
            sys.exit(0)
    else:
        fire_round(args.host, args.port, args.token)


if __name__ == "__main__":
    main()
