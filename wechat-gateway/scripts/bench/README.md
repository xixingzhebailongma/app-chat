# wechat-gateway 压测脚本（零依赖 Node）

对网关做三向链路验证 + 全端点覆盖 + 常规操作多次加压。全部用 Node 内置能力
（`fetch` / 全局 `WebSocket` / `crypto` HMAC），**无需安装任何依赖**（Node ≥ 22，本机 v26）。

## 前置

三件套已启动（mock 边侧 :8081 → 网关 :8080 → miniapp H5 :5173）：

```bash
bash scripts/dev/local-e2e.sh start    # 或 status 确认已在跑
```

## 运行

```bash
cd wechat-gateway
node scripts/bench/run-all.mjs            # 默认档
node scripts/bench/run-all.mjs --smoke     # 最小档（先跑这个验证脚本自身）
```

退出码：全部断言通过为 0，否则 1。报告 JSON 默认写到 `/tmp/bench-report.json`。

## 三方向

| 方向 | 验证内容 | 断言方式 |
|---|---|---|
| 1 边侧→前端 | 设备状态实时变 / 告警产生 | WS 观察器（前端替身）计数 `device_update`；`alerts/stats` 前后 unhandled 差值 |
| 2 前端→网关 | 高频接口负载正确 | 多并发档持续加压，断言无 5xx 且状态符合契约 |
| 3 网关→下游 | 设备/控制/场景/登录转发送达 | mock 日志增量命中数 == 转发数；cmd `echo` 回传；WS `cmd_response` 回推 |

## 全端点覆盖

覆盖 `/health`、`/api/miniapp/*`、`/api/admin/*`、`/api/notify/*`、`/api/oa/bind/*`、
`/internal/*` 全部端点，含负向断言（过期 JWT→401、教师跨空间→403、门禁 off 无 confirm→428、
门禁 toggle→400、错误内部令牌→401）。变更类端点 create→delete 配对，不污染状态。

## 可调参数（环境变量）

| 变量 | 默认 | 说明 |
|---|---|---|
| `BENCH_CONCURRENCY` | `8,32` | 方向 2 各端点的并发档位（逗号分隔） |
| `BENCH_DURATION_S` | `8` | 方向 2 每档持续秒数 |
| `BENCH_WS_OBSERVERS` | `3` | 方向 1 的 WS 观察器数量（前端替身） |
| `BENCH_WS_EVENTS` | `50` | 方向 1 注入 device_update 次数 |
| `BENCH_ALERT_EVENTS` | `20` | 方向 1 灌告警条数 |
| `BENCH_INGEST_RECORDS` | `20` | 方向 1 灌进出记录条数 |
| `BENCH_DOWNSTREAM_N` | `40` | 方向 3 设备列表/控制各发次数 |
| `BENCH_DOWNSTREAM_SCENE` | `20` | 方向 3 场景执行次数 |
| `BENCH_DOWNSTREAM_CONCURRENCY` | `16` | 方向 3 并发 |
| `BENCH_COVERAGE_N` | `3` | 全覆盖每个读端点重复次数 |
| `BENCH_REPORT` | `/tmp/bench-report.json` | 报告 JSON 路径 |
| `BASE` / `MOCK_BASE` / `INTERNAL_TOKEN` / `JWT_SECRET` / `MOCK_LOG` | 见下 | 连接与凭据 |

默认连接/凭据：`BASE=http://127.0.0.1:8080`、`MOCK_BASE=http://127.0.0.1:8081`、
`INTERNAL_TOKEN=dev-internal-token`、`JWT_SECRET=dev-shared-jwt-secret`、
`MOCK_LOG=/tmp/mock-go-backend.log`。

## 报告怎么读

- `PASS/FAIL` 行 = 每条断言；`STAT` 行 = RPS / 延迟分位（p50/p90/p99）/ 错误率。
- 三方向 FAIL 时先看 `FAIL` 行附带的 `[detail]`（状态码分布 / 命中数 / 实收条数）。
- 方向 3 的 mock 日志命中数应与转发数严格相等；不等说明有请求被网关吞掉或 mock 漏记。

## 已知观察点

- `/internal/notify/send` 每次后台 `std::thread().detach()` 发渠道，高并发会放大线程数。
- 告警冷却 1800s（键 `event_type+device_id+user+channel`），压测用 fresh `device_id` 规避。
- admin 首页/设备「全部」是 N+1 逐教室 `GET /devices`，本脚本方向 2 单教室视角，N+1 放大未覆盖。
- WS 代理 1:1（每前端连接 = 每边侧连接），高 WS 并发主要压 mock 与网关代理线程。

## 复位

内存模式写操作随轮次累加（告警/进出记录/运维日志）。要干净重跑：
`bash scripts/dev/local-e2e.sh stop && bash scripts/dev/local-e2e.sh start`。
