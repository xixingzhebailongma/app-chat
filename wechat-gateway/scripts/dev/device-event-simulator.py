#!/usr/bin/env python3
"""边侧设备事件模拟器 —— 验收清单「7.2 环节 1 边侧设备事件」的事件源。

扮演真实边侧设备（综合屏 / 传感器 / 人脸识别 / 外围设备）的事件源，向
微信通知网关的 `POST /internal/notify/send` 发送四类可推送事件，从而把
「设备产生事件 → 网关路由 → 推送」这条链路跑通、可演示。

四类事件与 7.6 推送条件（见 src/services/include/services/PushRule.h）：
    device_offline      transition == "online->offline"
    sensor_threshold    duration_sec >= 300
    face_login_failed   consecutive >= 3
    peripheral_offline  offline_minutes > 30

运行（先启动网关 ./build/wechat-gateway，监听 8080）：
    python3 scripts/dev/device-event-simulator.py --once            # 一次性发 4 类
    python3 scripts/dev/device-event-simulator.py --event face_login_failed --fresh-ids
    python3 scripts/dev/device-event-simulator.py --loop --interval 30   # 持续模拟

说明：网关对同一 event_type + device_id 在 30 分钟冷却期内去重（设计文档 9.3），
重复请求返回 final_status:"skipped"。默认用固定 device_id 以便观察冷却；
加 --fresh-ids 给 device_id 追加时间戳后缀，绕过冷却让事件再次推送。
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
SEVERITY = "warning"

# 四类可推送事件目录。每个 payload 都满足 7.6 推送条件，保证会被真正推送。
# device_id 保持固定，便于观察冷却去重；--fresh-ids 会追加时间戳后缀。
EVENTS = [
    {
        "event_type": "device_offline",
        "device_id": "dev_screen_01",
        "device_label": "综合屏",
        "content": "A101 综合屏已离线",
        "transition": "online->offline",
        "space_teachers": True,
    },
    {
        "event_type": "sensor_threshold",
        "device_id": "dev_sensor_01",
        "device_label": "温度传感器",
        "content": "A101 温度传感器持续超阈值 360 秒",
        "duration_sec": 360,
        "space_teachers": True,
    },
    {
        "event_type": "face_login_failed",
        "device_id": "dev_face_01",
        "device_label": "人脸识别",
        "content": "A101 人脸识别连续失败 3 次",
        "consecutive": 3,
        "space_teachers": False,
    },
    {
        "event_type": "peripheral_offline",
        "device_id": "dev_peripheral_01",
        "device_label": "外围传感器",
        "content": "A101 外围传感器离线 45 分钟",
        "offline_minutes": 45,
        "space_teachers": False,
    },
]


def build_payload(ev: dict, device_id: str) -> dict:
    """按 dto/NotifySendDto.h 的 NotifySendRequest 结构组装请求体。"""
    payload = {
        "event_id": f"evt_{ev['event_type']}_{time.time_ns()}",
        "event_type": ev["event_type"],
        "space_id": SPACE_ID,
        "device_id": device_id,
        "device_label": ev["device_label"],
        "severity": SEVERITY,
        "content": ev["content"],
        "target": {"roles": ["admin"], "space_teachers": ev["space_teachers"]},
    }
    for key in ("transition", "duration_sec", "consecutive", "offline_minutes"):
        if key in ev:
            payload[key] = ev[key]
    return payload


def send_event(host: str, port: int, token: str, payload: dict) -> bool:
    """向网关 /internal/notify/send 发送单个事件，返回是否成功送达。"""
    url = f"http://{host}:{port}/internal/notify/send"
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
            f"[event-simulator] 网关不可达 {url}: {e.reason}",
            file=sys.stderr,
            flush=True,
        )
        return False

    try:
        parsed = json.loads(body)
    except json.JSONDecodeError:
        parsed = body

    result = _format_result(parsed)
    print(
        f"[event-simulator] {payload['event_type']:<18} -> HTTP {status} {result}",
        flush=True,
    )
    return True


def _format_result(parsed) -> str:
    if isinstance(parsed, dict):
        bits = [
            f"{k}={parsed[k]}"
            for k in ("accepted", "notify_id", "final_status", "channels_tried")
            if k in parsed
        ]
        if bits:
            return " ".join(bits)
    return json.dumps(parsed, ensure_ascii=False)


def fire_round(events: list, host: str, port: int, token: str, fresh: bool) -> None:
    nonce = f"_{int(time.time())}" if fresh else ""
    for ev in events:
        device_id = ev["device_id"] + nonce
        send_event(host, port, token, build_payload(ev, device_id))


def main() -> None:
    parser = argparse.ArgumentParser(
        description="边侧设备事件模拟器（验收清单 7.2 环节 1）"
    )
    parser.add_argument("--host", default=DEFAULT_HOST, help="网关地址")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="网关端口")
    parser.add_argument(
        "--token",
        default=os.environ.get("INTERNAL_TOKEN", DEFAULT_TOKEN),
        help="内部令牌（默认 dev-internal-token，可用 INTERNAL_TOKEN 覆盖）",
    )
    parser.add_argument(
        "--once", action="store_true", help="一次性发完事件后退出（默认行为）"
    )
    parser.add_argument(
        "--loop", action="store_true", help="持续按 --interval 循环发送"
    )
    parser.add_argument(
        "--interval", type=float, default=30.0, help="--loop 模式下每轮间隔秒数"
    )
    parser.add_argument(
        "--event",
        choices=[e["event_type"] for e in EVENTS],
        help="只发送指定的一类事件",
    )
    parser.add_argument(
        "--fresh-ids",
        action="store_true",
        help="device_id 追加时间戳后缀，绕过 30 分钟冷却",
    )
    args = parser.parse_args()

    events = [e for e in EVENTS if args.event is None or e["event_type"] == args.event]

    if args.loop:
        print(
            f"[event-simulator] 持续模式：每 {args.interval}s 发一轮（Ctrl+C 停止）",
            flush=True,
        )
        try:
            while True:
                fire_round(events, args.host, args.port, args.token, fresh=True)
                time.sleep(args.interval)
        except KeyboardInterrupt:
            print("\n[event-simulator] 已停止", flush=True)
            sys.exit(0)
    else:
        fire_round(events, args.host, args.port, args.token, fresh=args.fresh_ids)


if __name__ == "__main__":
    main()
