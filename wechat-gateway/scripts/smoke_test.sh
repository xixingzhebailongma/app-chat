#!/usr/bin/env bash
#
# 冒烟测试（健康检查 → 登录 → 拉 spaces → 拉 devices → 执行场景 → 触发 mock
# 通知 → 家长绑定 mock），输出逐项 PASS/FAIL，全部通过退出码 0。
#
# 前置：网关已在 8080 监听（内存模式，无需 PostgreSQL/Redis）：
#     cd wechat-gateway && ./build/wechat-gateway
# 本脚本会自动拉起 scripts/mock-go-backend.py（若 8081 未监听），跑完自动回收。
#
# 说明：
#   - 「登录」用本地铸 openid_ticket + POST /api/miniapp/bind 走真实登录/绑定链
#     （login 的 jscode2session 需真实微信 appid，离线不可测，故跳过 login 直接
#     bind；等价于已登录）。
#   - 「触发 mock 通知」「家长绑定 mock」依赖网关 notify 渠道 mode=mock
#     （config.json 默认），dry-run 不触达真实微信/短信。
set -uo pipefail

BASE="${BASE:-http://127.0.0.1:8080}"
INTERNAL_TOKEN="${INTERNAL_TOKEN:-dev-internal-token}"
JWT_SECRET="${JWT_SECRET:-dev-shared-jwt-secret}"
MOCK_PORT="${MOCK_PORT:-8081}"
MOCK_LOG="${MOCK_LOG:-/tmp/smoke-mock-go-backend.log}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MOCK_PY="$SCRIPT_DIR/mock-go-backend.py"

PASS=0; FAIL=0
BODY=$(mktemp)
STATUS=""

# ---- HS256 JWT 铸造（与 src/utils/src/JwtUtil.cpp 一致）----
mint_jwt() {
  python3 - "$1" "$JWT_SECRET" <<'PY'
import sys, hmac, hashlib, base64, json, time
payload, secret = sys.argv[1], sys.argv[2]
def b64(b): return base64.urlsafe_b64encode(b).rstrip(b'=')
h = b64(json.dumps({"alg": "HS256", "typ": "JWT"}).encode())
p = b64(payload.encode())
s = b64(hmac.new(secret.encode(), h + b'.' + p, hashlib.sha256).digest())
print((h + b'.' + p + b'.' + s).decode())
PY
}

# openid_ticket：token_type=openid_ticket（绑定流程用，5 分钟有效）。
mint_openid_ticket() {
  mint_jwt "{\"openid\":\"$1\",\"token_type\":\"openid_ticket\",\"exp\":$(($(date +%s)+300))}"
}

get()  { STATUS=$(curl -s -o "$BODY" -w '%{http_code}' "$BASE$2" ${3:+ -H "$3"}); }
post() { STATUS=$(curl -s -o "$BODY" -w '%{http_code}' -H 'Content-Type: application/json' ${3:+ -H "$3"} -d "$2" "$BASE$1"); }
post_auth() { STATUS=$(curl -s -o "$BODY" -w '%{http_code}' -H "Authorization: Bearer $1" -H 'Content-Type: application/json' -d "$3" "$BASE$2"); }
get_auth()  { STATUS=$(curl -s -o "$BODY" -w '%{http_code}' -H "Authorization: Bearer $1" "$BASE$2"); }

check_status() {  # <label> <expected>
  if [ "$STATUS" = "$2" ]; then echo "PASS  $1"; PASS=$((PASS+1)); else echo "FAIL  $1 (status=$STATUS, body=$(cat "$BODY"))"; FAIL=$((FAIL+1)); fi
}
check() {  # <label> <python-bool-expr over $BODY>
  if python3 -c "import json,sys;d=json.load(open('$BODY'));sys.exit(0 if ($2) else 1)" 2>/dev/null; then
    echo "PASS  $1"; PASS=$((PASS+1))
  else
    echo "FAIL  $1 (body=$(cat "$BODY"))"; FAIL=$((FAIL+1))
  fi
}

cleanup_mock() {
  if [ -n "${MOCK_PID:-}" ]; then
    kill "$MOCK_PID" 2>/dev/null || true
    wait "$MOCK_PID" 2>/dev/null || true
  fi
}
trap cleanup_mock EXIT

# ---- 0. 前置检查：网关 /health 可达 ----
if ! curl -s -o /dev/null -w '%{http_code}' "$BASE/health" | grep -q 200; then
  echo "FAIL  gateway not reachable at $BASE (start ./build/wechat-gateway first)"
  exit 1
fi

# ---- 起 mock-go-backend（若未监听）----
if ! curl -s -o /dev/null "http://127.0.0.1:$MOCK_PORT/api/devices"; then
  python3 "$MOCK_PY" > "$MOCK_LOG" 2>&1 &
  MOCK_PID=$!
  sleep 1
fi

echo "=== 冒烟测试（BASE=$BASE）==="

# 1. 健康检查：返回 db/redis/channels 组件状态
get "" "/health"
check_status "1. 健康检查 -> 200" 200
check "1. 健康检查 body 含 service/db/redis/channels" \
  "set(['service','db','redis','channels','status']) <= set(d.keys())"
check "1. 各 notify 渠道状态已上报" \
  "set(['wechat_miniapp','wechat_oa','sms','dingtalk','wecom']) <= set(d['channels'].keys())"

# 2. 登录（bind 走真实登录/绑定链）
TICKET="$(mint_openid_ticket smoke_admin_openid)"
post "/api/miniapp/bind" '{"openid_token":"'"$TICKET"'","username":"admin","password":"admin123"}'
check_status "2. 登录(bind admin) -> 200" 200
ADMIN_TOKEN=$(python3 -c "import json;print(json.load(open('$BODY')).get('token',''))" 2>/dev/null)
if [ -z "$ADMIN_TOKEN" ]; then
  echo "FAIL  2. 未取到 access token"
  FAIL=$((FAIL+1))
else
  echo "PASS  2. 登录返回 access token"; PASS=$((PASS+1))
fi

# 3. 拉 spaces
get_auth "$ADMIN_TOKEN" "/api/miniapp/spaces"
check_status "3. 拉 spaces -> 200" 200
check "3. spaces 非空且含 space_id" "d['spaces'] and all('space_id' in s for s in d['spaces'])"

# 4. 拉 devices
get_auth "$ADMIN_TOKEN" "/api/miniapp/devices?space_id=spc_a8acdd5c"
check_status "4. 拉 devices -> 200" 200
check "4. devices 非空且含 device_id" "d['devices'] and all('device_id' in x for x in d['devices'])"

# 5. 执行场景
post_auth "$ADMIN_TOKEN" "/api/miniapp/scene/execute" '{"space_id":"spc_a8acdd5c","scene_id":"lesson_on"}'
check_status "5. 执行场景 lesson_on -> 200" 200
check "5. 场景 ok=true 且 applied>=1" "d.get('ok') is True and d.get('applied',0)>=1"

# 6. 触发 mock 通知（内部令牌 + 设备离线事件，miniapp 渠道 mode=mock dry-run）
post "/internal/notify/send" '{"event_type":"device_offline","space_id":"spc_a8acdd5c","device_id":"dev_screen_01","device_label":"综合屏","severity":"warning","content":"A101 综合屏已离线","transition":"online->offline","target":{"roles":["admin"]}}' "X-Internal-Token: $INTERNAL_TOKEN"
check_status "6. 触发 mock 通知 -> 200" 200
check "6. 通知被处理(success/partial/skipped)" "d.get('final_status') in ('success','partial','skipped')"

# 7. 家长绑定 mock（send-code 成功 + confirm 校验拒绝错误验证码）
post "/api/oa/bind/send-code" '{"phone":"13800000000"}'
check_status "7a. 家长绑定 send-code(mock) -> 200" 200
TICKET2="$(mint_openid_ticket smoke_parent_openid)"
post "/api/oa/bind/confirm" '{"openid_token":"'"$TICKET2"'","student_no":"20260101","phone":"13800000000","sms_code":"000000"}'
check_status "7b. 家长绑定 confirm(错误验证码) -> 400" 400

rm -f "$BODY"
echo
echo "=== 结果：PASS=$PASS FAIL=$FAIL ==="
[ "$FAIL" -eq 0 ]
