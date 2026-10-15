// 方向 2：miniapp 前端 → 网关（网关正确处理的负载）。
// 对高频前端接口按多个并发档位持续加压，断言无 5xx 且状态符合预期。
import { request, saturate, summarize, bearer } from '../lib/http.mjs';

const SPACE = 'spc_a8acdd5c';

export async function run(ctx) {
  const asserts = [];
  const stats = [];
  const levels = ctx.env.concurrency;
  const durMs = ctx.env.durationMs;
  const auth = bearer(ctx.adminJwt);
  const sceneId = ctx.sceneId;

  const eps = [
    { name: 'GET /health', method: 'GET', path: '/health', headers: {}, body: undefined },
    { name: 'GET /api/miniapp/me', method: 'GET', path: '/api/miniapp/me', headers: auth, body: undefined },
    { name: 'GET /api/miniapp/spaces', method: 'GET', path: '/api/miniapp/spaces', headers: auth, body: undefined },
    { name: 'GET /api/miniapp/space-types', method: 'GET', path: '/api/miniapp/space-types', headers: auth, body: undefined },
    { name: 'GET /api/miniapp/alerts', method: 'GET', path: `/api/miniapp/alerts?space_id=${SPACE}&page=1&page_size=20`, headers: auth, body: undefined },
    { name: 'GET /api/miniapp/alerts/stats', method: 'GET', path: '/api/miniapp/alerts/stats', headers: auth, body: undefined },
    { name: 'GET /api/miniapp/devices', method: 'GET', path: `/api/miniapp/devices?space_id=${SPACE}`, headers: auth, body: undefined },
    { name: 'POST /api/miniapp/device/control', method: 'POST', path: '/api/miniapp/device/control', headers: auth, body: { space_id: SPACE, device_type: 'zigbee', device_id: 'dev_c2936745', command: 'on' } },
  ];
  if (sceneId) {
    eps.push({ name: 'POST /api/miniapp/scenes/{id}/execute', method: 'POST', path: `/api/miniapp/scenes/${sceneId}/execute`, headers: auth, body: undefined });
  }

  for (const ep of eps) {
    for (const c of levels) {
      const res = await saturate(c, durMs, () => request({ method: ep.method, url: ctx.base + ep.path, headers: ep.headers, body: ep.body }));
      const sum = summarize(res, durMs);
      const no5xx = res.every((r) => r.status < 500);
      const all200 = res.every((r) => r.status === 200);
      const connFail = res.filter((r) => r.status === 0).length;
      const detail = `p50=${sum.p50.toFixed(0)} p90=${sum.p90.toFixed(0)} p99=${sum.p99.toFixed(0)}ms err=${(sum.errorRate * 100).toFixed(2)}% conn_fail=${connFail} ${JSON.stringify(sum.statuses)}`;
      stats.push({ label: `${ep.name} @c${c}`, value: `${sum.rps.toFixed(1)} rps`, detail });
      asserts.push({ label: `${ep.name} @c${c} 无 5xx`, pass: no5xx, detail });
      asserts.push({ label: `${ep.name} @c${c} 全 200`, pass: all200, detail });
    }
  }

  return { name: '方向2：前端 → 网关（负载 + 正确性）', asserts, stats };
}
