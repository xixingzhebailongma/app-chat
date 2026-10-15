// 方向 1：边侧 → 网关 → miniapp 前端（前端能看到相应变化）。
//  - 1a 设备状态实时：mock 注入 device_update → 网关 WS 透传 → 观察器（前端替身）收到。
//  - 1b 告警产生：/internal/notify/send → 落库 → 前端轮询 stats 可见 unhandled +N。
//  - 1c 进出记录回灌：/internal/access-records 幂等落库。
import { request, runPool, bearer, internal } from '../lib/http.mjs';
import { WsObserver } from '../lib/ws-observer.mjs';

const SPACE = 'spc_a8acdd5c';
const LIGHT = 'dev_c2936745';

export async function run(ctx) {
  const asserts = [];
  const stats = [];
  const t = (label, pass, detail = '') => asserts.push({ label, pass, detail });

  // ---- 1a 设备状态实时（WS）----
  const K = ctx.env.wsObservers;
  const N = ctx.env.wsEvents;
  const observers = [];
  for (let i = 0; i < K; i++) {
    const o = new WsObserver(ctx.wsUrl(SPACE, ctx.teacherJwt));
    await o.connect();
    observers.push(o);
  }
  t(`WS 观察器 x${K} 连接网关 /ws 成功`, observers.length === K);

  const injections = [];
  for (let i = 0; i < N; i++) {
    injections.push(() => request({
      method: 'POST',
      url: `${ctx.mockBase}/dev/device-update`,
      body: { space_id: SPACE, device_id: LIGHT, status: i % 2 ? 'offline' : 'online' },
    }));
  }
  const start = Date.now();
  const inj = await runPool(injections, Math.min(N, 20));
  const injMs = Date.now() - start;
  const injOk = inj.filter((r) => r.status === 200).length;
  t(`注入 ${N} 次 device_update（200 x${injOk}）`, injOk === N, `${injMs}ms`);

  let allGot = true;
  const got = [];
  for (const o of observers) {
    const ok = await o.waitFor('device_update', N, 15000);
    got.push(o.deviceUpdates);
    if (!ok) allGot = false;
    o.close();
  }
  t(`每个观察器都收到 ${N} 条 device_update（实收 ${got.join('/')}）`, allGot);
  stats.push({ label: 'WS 注入->送达 总耗时(ms)', value: injMs, detail: `${N} 次 / ${K} 观察器` });

  // ---- 1b 告警产生（落库 + 轮询可见）----
  const statsUrl = `${ctx.base}/api/miniapp/alerts/stats`;
  const auth = bearer(ctx.adminJwt);
  const s0 = await request({ url: statsUrl, headers: auth });
  const c0 = s0.json?.by_status?.unhandled ?? null;

  const M = ctx.env.alertEvents;
  const types = ['device_offline', 'sensor_threshold', 'face_login_failed', 'peripheral_offline'];
  const ts = Date.now();
  const sends = [];
  for (let i = 0; i < M; i++) {
    sends.push(() => request({
      method: 'POST',
      url: `${ctx.base}/internal/notify/send`,
      headers: internal(ctx.internalToken),
      body: {
        event_type: types[i % types.length],
        content: `压测告警 ${i}`,
        target: { roles: ['admin'] },
        space_id: SPACE,
        device_id: `dev_bench_${ts}_${i}`,
        transition: 'online->offline',
        event_id: `alt_bench_${ts}_${i}`,
      },
    }));
  }
  const sendRes = await runPool(sends, Math.min(M, 16));
  const sendOk = sendRes.filter((r) => r.status === 200).length;
  t(`notify/send ${M} 次全部 200`, sendOk === M, `实际 ${sendOk}`);

  const s1 = await request({ url: statsUrl, headers: auth });
  const c1 = s1.json?.by_status?.unhandled ?? null;
  const delta = c0 !== null && c1 !== null ? c1 - c0 : null;
  t(`未处理告警数 +${M}（before=${c0} after=${c1} delta=${delta}）`, delta === M);

  // ---- 1c 内部进出记录回灌（event_id 幂等）----
  const R = ctx.env.ingestRecords;
  const recs = [];
  for (let i = 0; i < R; i++) {
    recs.push(() => request({
      method: 'POST',
      url: `${ctx.base}/internal/access-records`,
      headers: internal(ctx.internalToken),
      body: {
        event_id: `evt_bench_${ts}_${i}`,
        space_id: SPACE,
        occurred_at: new Date().toISOString(),
        name: '压测学生',
        auth_type: 'face',
        result: 'login',
        device_id: LIGHT,
      },
    }));
  }
  const recRes = await runPool(recs, Math.min(R, 16));
  t(`access-records ${R} 次全部 200`, recRes.every((r) => r.status === 200));

  return { name: '方向1：边侧 → 网关 → 前端（可见变化）', asserts, stats };
}
