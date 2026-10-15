#!/usr/bin/env node
// wechat-gateway 压测编排：预热检查 → 三方向 → 全端点覆盖 → 报告。
//
// 用法（在 wechat-gateway/ 目录下）：
//   node scripts/bench/run-all.mjs            # 默认档
//   node scripts/bench/run-all.mjs --smoke     # 最小档（脚本自身冒烟）
//   BENCH_CONCURRENCY=8,32,64 node scripts/bench/run-all.mjs
//
// 全部参数走环境变量，见 README.md。
import { request, bearer } from './lib/http.mjs';
import { mintJwt, mintExpiredJwt } from './lib/mint.mjs';
import { render } from './lib/report.mjs';
import { snapshotLog } from './lib/downstream.mjs';

const SMOKE = process.argv.includes('--smoke');

const num = (v, d) => (v === undefined || v === '' ? d : Number(v));
const numList = (v, d) => (v === undefined || v === '' ? d : v.split(',').map((x) => Number(x)));

const env = {
  concurrency: SMOKE ? [2, 4] : numList(process.env.BENCH_CONCURRENCY, [8, 32]),
  durationMs: SMOKE ? 1500 : num(process.env.BENCH_DURATION_S, 8) * 1000,
  wsObservers: SMOKE ? 2 : num(process.env.BENCH_WS_OBSERVERS, 3),
  wsEvents: SMOKE ? 10 : num(process.env.BENCH_WS_EVENTS, 50),
  alertEvents: SMOKE ? 8 : num(process.env.BENCH_ALERT_EVENTS, 20),
  ingestRecords: SMOKE ? 8 : num(process.env.BENCH_INGEST_RECORDS, 20),
  downstreamN: SMOKE ? 10 : num(process.env.BENCH_DOWNSTREAM_N, 40),
  downstreamScene: SMOKE ? 6 : num(process.env.BENCH_DOWNSTREAM_SCENE, 20),
  downstreamConcurrency: SMOKE ? 4 : num(process.env.BENCH_DOWNSTREAM_CONCURRENCY, 16),
  coverageN: SMOKE ? 1 : num(process.env.BENCH_COVERAGE_N, 3),
};

const base = process.env.BASE || 'http://127.0.0.1:8080';
const mockBase = process.env.MOCK_BASE || 'http://127.0.0.1:8081';
const internalToken = process.env.INTERNAL_TOKEN || 'dev-internal-token';
const mockLog = process.env.MOCK_LOG || '/tmp/mock-go-backend.log';
const reportPath = process.env.BENCH_REPORT || '/tmp/bench-report.json';

const ctx = {
  base,
  mockBase,
  wsBase: base.replace(/^http/, 'ws'),
  internalToken,
  adminJwt: mintJwt('u_admin_1', 'admin'),
  teacherJwt: mintJwt('u_teacher_1', 'teacher'),
  expiredJwt: mintExpiredJwt('u_admin_1', 'admin'),
  mockLog,
  sceneId: null,
  env,
  wsUrl: (spaceId, jwt) => `${base.replace(/^http/, 'ws')}/ws?token=${encodeURIComponent(jwt)}&space_id=${encodeURIComponent(spaceId)}`,
};

async function resolveSceneId() {
  const r = await request({ url: `${base}/api/miniapp/scenes?space_id=spc_a8acdd5c`, headers: bearer(ctx.adminJwt) });
  if (r.status !== 200) return null;
  const scenes = r.json?.scenes || [];
  return (scenes.find((s) => s.kind === 'all_on') || scenes[0])?.scene_id ?? null;
}

async function preflight() {
  const checks = [];
  const h = await request({ url: `${base}/health` });
  checks.push({ label: '网关 /health -> 200', ok: h.status === 200, detail: `status=${h.status}` });

  const me = await request({ url: `${base}/api/miniapp/me`, headers: bearer(ctx.adminJwt) });
  checks.push({ label: 'admin JWT /me -> 200', ok: me.status === 200, detail: `status=${me.status}` });

  const m = await request({ url: `${mockBase}/api/devices?space_id=spc_a8acdd5c` });
  checks.push({ label: 'mock 边侧可及', ok: m.status === 200, detail: `status=${m.status}` });

  const snap = snapshotLog(mockLog);
  checks.push({ label: 'mock 日志文件存在（下游送达证据源）', ok: !snap.missing, detail: snap.missing ? `缺失 ${mockLog}` : `${mockLog} (${snap.len}B)` });

  ctx.sceneId = await resolveSceneId();
  checks.push({ label: '场景 all_on 已懒种并可执行', ok: !!ctx.sceneId, detail: ctx.sceneId ? `scene_id=${ctx.sceneId}` : 'GET scenes 未返回可用场景' });

  let ok = true;
  console.log('=== 预热检查 ===');
  for (const c of checks) {
    if (!c.ok) ok = false;
    console.log(`  ${c.ok ? 'OK ' : 'BAD'}  ${c.label}${c.detail ? `  [${c.detail}]` : ''}`);
  }
  if (!ok) {
    console.error('\n预热检查未通过，终止。请确认三件套已启动（scripts/dev/local-e2e.sh start）。');
    process.exit(1);
  }
}

const scenarios = [
  (await import('./scenarios/direction1.mjs')).run,
  (await import('./scenarios/direction2.mjs')).run,
  (await import('./scenarios/direction3.mjs')).run,
  (await import('./scenarios/full-coverage.mjs')).run,
];

await preflight();

const results = [];
for (const run of scenarios) results.push(await run(ctx));

const ok = render(results, { outJsonPath: reportPath });
process.exit(ok ? 0 : 1);
