#!/usr/bin/env bash
# ============================================================================
# 重置本地网关 PG 到「迁移 + 种子」基线，用于多人协作时对齐共享演示数据。
#
# 用法（任意目录执行均可，脚本自动定位仓库根）：
#   bash wechat-gateway/scripts/dev/reset-db.sh              # 重置并重启网关
#   bash wechat-gateway/scripts/dev/reset-db.sh --no-restart # 只重置，不重启网关
#
# 环境变量覆盖（默认匹配本机 /tmp/gw-pg 联调集群）：
#   PGHOST=127.0.0.1  PGPORT=5433  PGUSER=kai  PGDB=gateway
#   PG_BIN=/usr/lib/postgresql/14/bin
#   LLM_BASE_URL=http://192.168.102.128:8000/v1  LLM_MODEL=qwen3.6  LLM_API_KEY=sk-xxx
#
# 动作：停网关 → DROP/CREATE 数据库 → 按序跑 migration_v*.sql → 跑 seed_dev.sql
#       →（默认）重启网关。seed 是幂等的，但 DROP 才能清掉「种子之外的多余数据」。
# ============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
GW_DIR="$REPO_ROOT/wechat-gateway"
GW_BIN="$GW_DIR/build/wechat-gateway"

PGHOST="${PGHOST:-127.0.0.1}"
PGPORT="${PGPORT:-5433}"
PGUSER="${PGUSER:-kai}"
PGDB="${PGDB:-gateway}"
PG_BIN="${PG_BIN:-/usr/lib/postgresql/14/bin}"
PSQL="$PG_BIN/psql"

LLM_BASE_URL="${LLM_BASE_URL:-http://192.168.102.128:8000/v1}"
LLM_MODEL="${LLM_MODEL:-qwen3.6}"
LLM_API_KEY="${LLM_API_KEY:-sk-xxx}"

RESTART=1
[ "${1:-}" = "--no-restart" ] && RESTART=0

log()  { printf '\033[1;34m[reset-db]\033[0m %s\n' "$*"; }
ok()   { printf '\033[1;32m  ✔ %s\033[0m\n' "$*"; }
die()  { printf '\033[1;31m  ✗ %s\033[0m\n' "$*" >&2; exit 1; }

# 1) 停网关（DROP DATABASE 要求无活动连接）。
GW_PID="$(pgrep -x wechat-gateway 2>/dev/null || true)"
if [ -n "$GW_PID" ]; then
  log "停止网关（PID $GW_PID）…"
  kill "$GW_PID" 2>/dev/null || true
  sleep 1
  ok "网关已停"
else
  log "网关未在运行，跳过停止"
fi

# 2) 检查 PG 可达，然后重建数据库。
if ! "$PSQL" -h "$PGHOST" -p "$PGPORT" -U "$PGUSER" -d postgres -c "SELECT 1" >/dev/null 2>&1; then
  die "连不上 PostgreSQL（$PGHOST:$PGPORT user=$PGUSER）。先起集群：pg_ctl -D /tmp/gw-pg -o '-p 5433 -c listen_addresses=127.0.0.1 -c unix_socket_directories=/tmp' start"
fi
log "重建数据库 $PGDB …"
"$PSQL" -h "$PGHOST" -p "$PGPORT" -U "$PGUSER" -d postgres -c "DROP DATABASE IF EXISTS $PGDB;" >/dev/null
"$PSQL" -h "$PGHOST" -p "$PGPORT" -U "$PGUSER" -d postgres -c "CREATE DATABASE $PGDB;" >/dev/null
ok "已重建 $PGDB"

# 3) 按序跑迁移（sort -V 保证 v2 在 v10 之前）。
log "应用迁移 …"
for f in $(ls "$GW_DIR/sql"/migration_v*.sql | sort -V); do
  "$PSQL" -h "$PGHOST" -p "$PGPORT" -U "$PGUSER" -d "$PGDB" -f "$f" >/dev/null || die "迁移失败：$(basename "$f")"
  ok "$(basename "$f")"
done

# 4) 跑种子。
log "应用种子数据 seed_dev.sql …"
"$PSQL" -h "$PGHOST" -p "$PGPORT" -U "$PGUSER" -d "$PGDB" -f "$GW_DIR/sql/seed_dev.sql" >/dev/null || die "种子失败"
ok "seed_dev.sql 完成"

# 5)（默认）重启网关。
if [ "$RESTART" -eq 1 ]; then
  if [ ! -x "$GW_BIN" ]; then
    die "找不到 $GW_BIN，先构建：cd $GW_DIR && cmake --build build -j"
  fi
  log "重启网关（PG 模式 + 本地 Qwen）…"
  (cd "$GW_DIR" && PG_CONNINFO="host=$PGHOST port=$PGPORT dbname=$PGDB user=$PGUSER" \
      LLM_BASE_URL="$LLM_BASE_URL" LLM_MODEL="$LLM_MODEL" LLM_API_KEY="$LLM_API_KEY" \
      nohup "$GW_BIN" > /tmp/gateway.log 2>&1 </dev/null &)
  ok "网关已重启（日志 /tmp/gateway.log）"
else
  log "按 --no-restart，未重启网关（之后可用 local-e2e.sh start 或手动起）"
fi

log "完成。教师 li→A101、wang→多媒体报告厅；各业务面种子已就绪。"
