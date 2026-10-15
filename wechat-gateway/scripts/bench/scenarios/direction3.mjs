// 方向 3：网关 → 下游（边侧 go-backend 收到）。
// 对转发类接口并发发 N 次，断言：网关透传 200 + mock 日志增量命中数一致 +
// WS 观察器收到 cmd_response（下游回推闭环）。
import { request, runPool, bearer } from '../lib/http.mjs';
import { WsObserver } from '../lib/ws-observer.mjs';
import { snapshotLog, deltaLog, countMatches } from '../lib/downstream.mjs';
import { mintOpenidTicket } from '../lib/mint.mjs';

const SPACE = 'spc_a8acdd5c';
const LIGHT = 'dev_c2936745';

export async function run(ctx) {
  const asserts = [];
  const stats = [];
  const auth = bearer(ctx.adminJwt);

  const prev = snapshotLog(ctx.mockLog);
  const obs = new WsObserver(ctx.wsUrl(SPACE, ctx.adminJwt));
  await obs.connect();

  const Ndev = ctx.env.downstreamN;
  const Nctrl = ctx.env.downstreamN;
  const Nscene = ctx.env.downstreamScene;
  const conc = ctx.env.downstreamConcurrency;

  // 设备列表（每次 1 次下游 GET /api/devices）
  const devRes = await runPool(
    Array.from({ length: Ndev }, () => () => request({ url: `${ctx.base}/api/miniapp/devices?space_id=${SPACE}`, headers: auth })),
    conc,
  );
  const devOk = devRes.filter((r) => r.status === 200).length;

  // 设备控制（每次 1 次下游 POST /api/devices/{t}/{id}/cmd + 1 次 cmd_response）
  const ctrlRes = await runPool(
    Array.from({ length: Nctrl }, () => () => request({
      method: 'POST',
      url: `${ctx.base}/api/miniapp/device/control`,
      headers: auth,
      body: { space_id: SPACE, device_type: 'zigbee', device_id: LIGHT, command: 'on' },
    })),
    conc,
  );
  const ctrlOk = ctrlRes.filter((r) => r.status === 200).length;
  const ctrlEcho = ctrlRes.filter((r) => r.json?.echo).length;

  // 场景执行（每次 applied 个下游 cmd）
  const sceneRes = await runPool(
    Array.from({ length: Nscene }, () => () => request({ method: 'POST', url: `${ctx.base}/api/miniapp/scenes/${ctx.sceneId}/execute`, headers: auth })),
    conc,
  );
  const sceneOk = sceneRes.filter((r) => r.status === 200).length;
  const totalApplied = sceneRes.reduce((s, r) => s + (r.json?.applied ?? 0), 0);

  // bind（1 次下游 POST /api/auth/login；fresh openid 避免唯一约束冲突）
  const openid = `openid_bench_${Date.now()}`;
  const bindRes = await request({
    method: 'POST',
    url: `${ctx.base}/api/miniapp/bind`,
    body: { openid_token: mintOpenidTicket(openid), username: 'li', password: '123456' },
  });
  const bindOk = bindRes.status === 200;

  const d = deltaLog(prev);
  const devicesHits = countMatches(d, /\[mock-go-backend\] GET \/api\/devices/g);
  const cmdHits = countMatches(d, /\[mock-go-backend\] POST \/api\/devices\/[^\n]*\/cmd/g);
  const loginHits = countMatches(d, /\[mock-go-backend\] POST \/api\/auth\/login/g);

  const cmdResponseSeen = await obs.waitFor('cmd_response', Nctrl, 15000);
  obs.close();

  const logOk = !prev.missing;
  asserts.push({ label: `devices 全 200 (${devOk}/${Ndev})`, pass: devOk === Ndev });
  asserts.push({ label: `control 全 200 且 echo 回传 (${ctrlEcho}/${Nctrl})`, pass: ctrlOk === Nctrl && ctrlEcho === Nctrl });
  asserts.push({ label: `scene execute 全 200 (${sceneOk}/${Nscene}) applied 合计 ${totalApplied}`, pass: sceneOk === Nscene && totalApplied >= Nscene });
  asserts.push({ label: `bind 200 (${bindRes.status})`, pass: bindOk });
  // 场景执行会先枚举设备（all_on/all_off 规则匹配需设备列表），每次 execute 多 1 次 GET /api/devices。
  const expectedDevices = Ndev + Nscene;
  asserts.push({ label: `下游日志 GET /api/devices 命中 ${devicesHits} == ${expectedDevices}`, pass: logOk && devicesHits === expectedDevices, detail: `直接列表 ${Ndev} + 场景枚举 ${Nscene}${logOk ? '' : '；mock 日志缺失'}` });
  asserts.push({ label: `下游日志 POST /api/devices/*/cmd 命中 ${cmdHits} == ${Nctrl}+${totalApplied}`, pass: logOk && cmdHits === Nctrl + totalApplied });
  asserts.push({ label: `下游日志 POST /api/auth/login 命中 ${loginHits} == 1`, pass: logOk && loginHits === 1 });
  asserts.push({ label: `WS 观察器收到 cmd_response >= ${Nctrl}（下游回推）`, pass: cmdResponseSeen });

  return { name: '方向3：网关 → 下游（送达证据）', asserts, stats };
}
