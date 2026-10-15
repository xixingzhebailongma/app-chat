#!/usr/bin/env bash
# ============================================================================
# 本地联调一键脚本：起「mock 边侧 + C++ 网关 + miniapp H5」三件套，
# 并打印登录注入行与触发示例。
#
# 用法（在任意目录执行均可，脚本会自动定位仓库根目录）：
#   bash wechat-gateway/scripts/dev/local-e2e.sh start     # 起栈（默认动作）
#   bash wechat-gateway/scripts/dev/local-e2e.sh stop      # 停栈
#   bash wechat-gateway/scripts/dev/local-e2e.sh status    # 看状态
#
# 说明：
#   - 网关以 dev 内存模式启动（无需 PG/Redis/真实凭证）
#   - 前端 VITE_USE_MOCK=false + VITE_BASE_URL 置空 → 走 Vite 代理（同源，无 CORS）
#   - 日志：/tmp/wechat-gateway.log  /tmp/mock-go-backend.log  /tmp/miniapp-dev.log
# ============================================================================
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
GW_DIR="$REPO_ROOT/wechat-gateway"
MP_DIR="$REPO_ROOT/miniapp"
GW_BIN="$GW_DIR/build/wechat-gateway"

LOG_DIR="${TMP_LOG_DIR:-/tmp}"
GW_LOG="$LOG_DIR/wechat-gateway.log"
MOCK_LOG="$LOG_DIR/mock-go-backend.log"
MP_LOG="$LOG_DIR/miniapp-dev.log"

MOCK_PORT=8081
GW_PORT=8080
MP_PORT=5173

log()  { printf '\033[1;34m[local-e2e]\033[0m %s\n' "$*"; }
ok()   { printf '\033[1;32m  ✔ %s\033[0m\n' "$*"; }
warn() { printf '\033[1;33m  ! %s\033[0m\n' "$*"; }

port_busy() {  # $1=port
  ss -ltn 2>/dev/null | grep -q ":$1 "
}

wait_port() {  # $1=port
  local i
  for i in $(seq 1 30); do
    port_busy "$1" && return 0
    sleep 0.5
  done
  return 1
}

start_mock() {
  if port_busy "$MOCK_PORT"; then
    ok "mock-go-backend 已在运行（:$MOCK_PORT）"
    return 0
  fi
  log "启动 mock-go-backend（:$MOCK_PORT，HTTP + WebSocket）…"
  (cd "$GW_DIR" && nohup python3 scripts/mock-go-backend.py > "$MOCK_LOG" 2>&1 &)
  wait_port "$MOCK_PORT" && ok "mock-go-backend 就绪" || warn "启动失败，看 $MOCK_LOG"
}

start_gateway() {
  if port_busy "$GW_PORT"; then
    ok "wechat-gateway 已在运行（:$GW_PORT）"
    return 0
  fi
  if [ ! -x "$GW_BIN" ]; then
    warn "找不到 $GW_BIN，先构建：cd $GW_DIR && cmake --build build -j"
    return 1
  fi
  log "启动 wechat-gateway（:$GW_PORT，dev 内存模式）…"
  (cd "$GW_DIR" && nohup "$GW_BIN" > "$GW_LOG" 2>&1 &)
  wait_port "$GW_PORT" && ok "wechat-gateway 就绪" || warn "启动失败，看 $GW_LOG"
}

start_mp() {
  if port_busy "$MP_PORT"; then
    ok "miniapp H5 dev server 已在运行（:$MP_PORT）"
    return 0
  fi
  log "启动 miniapp H5 dev server（:$MP_PORT）…"
  (cd "$MP_DIR" && nohup npm run dev:h5 > "$MP_LOG" 2>&1 &)
  wait_port "$MP_PORT" && ok "miniapp 就绪" || warn "启动失败，看 $MP_LOG"
}

stop() {
  log "停止三件套…"
  pkill -f "mock-go-backend.py" 2>/dev/null && ok "已停 mock-go-backend" || warn "mock-go-backend 未在运行"
  pkill -f "build/wechat-gateway" 2>/dev/null && ok "已停 wechat-gateway" || warn "wechat-gateway 未在运行"
  pkill -f "miniapp/node_modules/.bin/uni" 2>/dev/null && ok "已停 miniapp dev" || warn "miniapp 未在运行"
}

status() {
  echo "---- 服务状态 ----"
  if curl -s -o /dev/null -w "%{http_code}" "http://127.0.0.1:$GW_PORT/health" 2>/dev/null | grep -q 200; then
    ok "网关   :$GW_PORT  HTTP 200"
  else
    warn "网关   :$GW_PORT  不可达"
  fi
  if curl -s -o /dev/null -w "%{http_code}" "http://127.0.0.1:$MP_PORT/" 2>/dev/null | grep -q 200; then
    ok "前端   :$MP_PORT  HTTP 200"
  else
    warn "前端   :$MP_PORT  不可达"
  fi
  port_busy "$MOCK_PORT" && ok "边侧   :$MOCK_PORT  已监听" || warn "边侧   :$MOCK_PORT  未监听"
}

print_inject() {
  echo
  echo "================ 登录注入行（浏览器 F12 → Console 粘贴，选「管理员」那行） ================"
  (cd "$GW_DIR" && python3 scripts/dev/mint-session.py)
}

print_triggers() {
  local LAN_IP
  LAN_IP="$(hostname -I 2>/dev/null | awk '{print $1}')"
  echo
  echo "================ 触发示例 ================"
  echo "# ① 设备状态实时变（改 status: offline/online，前端不刷新就变）"
  cat <<'EOF'
curl -X POST 127.0.0.1:8081/dev/device-update \
  -H 'Content-Type: application/json' \
  -d '{"space_id":"spc_a8acdd5c","device_id":"dev_c2936745","status":"offline"}'
EOF
  echo
  echo "# ② 告警（前端告警页/首页角标约 10s 内 +1）"
  echo "python3 \"$GW_DIR/scripts/dev/device-event-simulator.py\" --once --fresh-ids"
  echo
  echo "# 浏览器访问（Windows 直接打开；打不开用局域网 IP）"
  echo "   http://localhost:$MP_PORT/"
  [ -n "$LAN_IP" ] && echo "   http://$LAN_IP:$MP_PORT/"
}

case "${1:-start}" in
  start)
    log "启动本地联调三件套（mock 边侧 :$MOCK_PORT → 网关 :$GW_PORT → miniapp H5 :$MP_PORT）"
    start_mock
    start_gateway
    start_mp
    sleep 1
    status
    print_inject
    print_triggers
    ;;
  stop)
    stop
    ;;
  status)
    status
    ;;
  *)
    echo "用法: $0 {start|stop|status}   （默认 start）"
    ;;
esac
