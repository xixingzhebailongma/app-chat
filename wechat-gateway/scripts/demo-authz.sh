#!/usr/bin/env bash
# 演示 space 鉴权 + alerts 角色过滤。
# 前置：gateway 已在 8080（./build/wechat-gateway）、mock-go-backend 已在 8081。
set -uo pipefail

BASE=http://localhost:8080

# 铸 JWT（与 go-backend 共享 secret dev-shared-jwt-secret）
mint() {  # $1=user_id  $2=role
    python3 -c "import hmac,hashlib,base64,json,time,sys
s='dev-shared-jwt-secret'
b=lambda x:base64.urlsafe_b64encode(x).rstrip(b'=').decode()
h=b(json.dumps({'alg':'HS256','typ':'JWT'},separators=(',',':')).encode())
p=b(json.dumps({'user_id':sys.argv[1],'role':sys.argv[2],'exp':int(time.time())+259200},separators=(',',':')).encode())
print(h+'.'+p+'.'+b(hmac.new(s.encode(),(h+'.'+p).encode(),hashlib.sha256).digest()))" "$1" "$2"
}

TEACHER=$(mint u_teacher_1 teacher)
ADMIN=$(mint u_admin_1 admin)

req() {  # $1=label  $2=path  $3=token
    local code body
    code=$(curl -s -o /tmp/demo_body -w '%{http_code}' "$BASE$2" -H "Authorization: Bearer $3")
    body=$(cat /tmp/demo_body)
    printf '\n【%s】\n  GET %s\n  HTTP %s\n  → %s\n' "$1" "$2" "$code" "$body"
}

echo "==================== 演示一：李老师（teacher）的视角 ===================="
req "打开自己教室 A101 的设备列表" "/api/miniapp/devices?space_id=spc_a8acdd5c" "$TEACHER"
req "尝试打开别人教室 B202（应被拒 403）" "/api/miniapp/devices?space_id=spc_b7bee44d" "$TEACHER"
req "查看告警（应只有 A101 的 2 条，且处理人/备注脱敏为空）" "/api/miniapp/alerts" "$TEACHER"

echo
echo "==================== 演示二：管理员（admin）的视角 ===================="
req "打开任意教室 B202 的设备列表" "/api/miniapp/devices?space_id=spc_b7bee44d" "$ADMIN"
req "查看告警（3 条全量，处理人/备注可见）" "/api/miniapp/alerts" "$ADMIN"
