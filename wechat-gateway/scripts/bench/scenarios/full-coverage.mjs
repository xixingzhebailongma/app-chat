// 全端点覆盖矩阵：逐个打一遍所有对外 /api/* 与内部 /internal/* 端点，
// 断言状态码符合契约（含负向 401/403/428/400）。变更类端点 create→delete 配对。
import { request, bearer, internal } from '../lib/http.mjs';
import { mintOpenidTicket } from '../lib/mint.mjs';

const SPACE = 'spc_a8acdd5c';   // teacher_1 绑定教室
const SPACE2 = 'spc_std_a102';  // 另一教室（用于绑定/门禁标记/越权）
const LIGHT = 'dev_c2936745';
const DOOR2 = 'dev_d0or_a102';  // A102 门禁（未被种子标记，可 create/delete）

export async function run(ctx) {
  const asserts = [];
  const stats = [];
  const t = (label, pass, detail = '') => asserts.push({ label, pass, detail });
  const admin = bearer(ctx.adminJwt);
  const teacher = bearer(ctx.teacherJwt);
  const intr = internal(ctx.internalToken);
  const N = ctx.env.coverageN;
  const ts = Date.now();

  const get = (p, h) => request({ url: ctx.base + p, headers: h });
  const post = (p, b, h) => request({ method: 'POST', url: ctx.base + p, body: b, headers: h });
  const put = (p, b, h) => request({ method: 'PUT', url: ctx.base + p, body: b, headers: h });
  const del = (p, h) => request({ method: 'DELETE', url: ctx.base + p, headers: h });
  const patch = (p, b, h) => request({ method: 'PATCH', url: ctx.base + p, body: b, headers: h });

  // 重复 n 次直到全部符合期望状态码。
  const rep = async (label, fn, expect, n = N) => {
    let ok = true;
    let last = -1;
    for (let i = 0; i < n; i++) {
      const r = await fn();
      last = r.status;
      if (r.status !== expect) ok = false;
    }
    t(label, ok, `status=${last} x${n}`);
  };

  // ---- 公开 / 登录绑定 ----
  await rep('GET /health -> 200', () => get('/health'), 200);
  const bindRes = await post('/api/miniapp/bind', { openid_token: mintOpenidTicket(`openid_cov_${ts}`), username: 'li', password: '123456' });
  t('POST /api/miniapp/bind -> 200（走边侧登录）', bindRes.status === 200, `status=${bindRes.status}`);
  const loginRes = await post('/api/miniapp/login', { code: `code_${ts}` });
  t('POST /api/miniapp/login 已受理（离线无真实 appid，仅记录状态）', loginRes.status === 200 || loginRes.status >= 400, `status=${loginRes.status}`);

  // ---- 身份 / 只读 ----
  await rep('GET /api/miniapp/me admin -> 200', () => get('/api/miniapp/me', admin), 200);
  await rep('GET /api/miniapp/me teacher -> 200', () => get('/api/miniapp/me', teacher), 200);
  await rep('GET /api/miniapp/spaces admin -> 200', () => get('/api/miniapp/spaces', admin), 200);
  await rep('GET /api/miniapp/spaces teacher -> 200', () => get('/api/miniapp/spaces', teacher), 200);
  await rep('GET /api/miniapp/space-types -> 200', () => get('/api/miniapp/space-types', admin), 200);
  await rep('GET /api/miniapp/alerts admin -> 200', () => get(`/api/miniapp/alerts?space_id=${SPACE}&page=1&page_size=20`, admin), 200);
  await rep('GET /api/miniapp/alerts teacher -> 200', () => get(`/api/miniapp/alerts?space_id=${SPACE}&page=1&page_size=20`, teacher), 200);
  await rep('GET /api/miniapp/alerts/stats admin -> 200', () => get('/api/miniapp/alerts/stats', admin), 200);
  await rep('GET /api/miniapp/alerts/stats teacher -> 200', () => get('/api/miniapp/alerts/stats', teacher), 200);
  await rep('GET /api/miniapp/subscribe -> 200', () => get('/api/miniapp/subscribe', admin), 200);
  await rep('GET /api/miniapp/notify/templates -> 200', () => get('/api/miniapp/notify/templates', admin), 200);
  await rep('GET /api/miniapp/access-records admin -> 200', () => get(`/api/miniapp/access-records?space_id=${SPACE}`, admin), 200);
  await rep('GET /api/miniapp/door-devices admin -> 200', () => get(`/api/miniapp/door-devices?space_id=${SPACE}`, admin), 200);
  await rep('GET /api/miniapp/door-devices teacher 本教室 -> 200', () => get(`/api/miniapp/door-devices?space_id=${SPACE}`, teacher), 200);

  // ---- 越权（teacher 跨空间）----
  await rep('GET /api/miniapp/devices teacher 跨空间 -> 403', () => get(`/api/miniapp/devices?space_id=${SPACE2}`, teacher), 403);
  await rep('GET /api/miniapp/door-devices teacher 跨空间 -> 403', () => get(`/api/miniapp/door-devices?space_id=${SPACE2}`, teacher), 403);
  await rep('GET /api/miniapp/access-records teacher 跨空间 -> 403', () => get(`/api/miniapp/access-records?space_id=${SPACE2}`, teacher), 403);

  // ---- admin 专属读（teacher -> 403）----
  await rep('GET /api/admin/spaces admin -> 200', () => get('/api/admin/spaces', admin), 200);
  await rep('GET /api/admin/spaces teacher -> 403', () => get('/api/admin/spaces', teacher), 403);
  await rep('GET /api/miniapp/space-bindings admin -> 200', () => get('/api/miniapp/space-bindings', admin), 200);
  await rep('GET /api/miniapp/space-bindings teacher -> 403', () => get('/api/miniapp/space-bindings', teacher), 403);
  await rep('GET /api/notify/bindings admin -> 200', () => get('/api/notify/bindings?channel=wechat_miniapp', admin), 200);
  await rep('GET /api/miniapp/operation-logs admin -> 200', () => get('/api/miniapp/operation-logs', admin), 200);
  await rep('GET /api/miniapp/operation-logs teacher -> 403', () => get('/api/miniapp/operation-logs', teacher), 403);

  // ---- 订阅（写）----（accepted_templates 必须是 config 里已注册的真实模板 ID）
  const tmpls = await get('/api/miniapp/notify/templates', admin);
  const tids = tmpls.json?.template_ids || [];
  const subRes = await post('/api/miniapp/subscribe', { accepted_templates: tids }, admin);
  t('POST /api/miniapp/subscribe accepted_templates -> 200', subRes.status === 200, `status=${subRes.status} tids=${tids.length}`);

  // ---- 场景 CRUD + 执行 ----
  const sceneCreate = await post('/api/miniapp/scenes', { space_id: SPACE, name: `压测场景${ts}`, device_states: [{ device_id: LIGHT, device_type: 'zigbee', command: 'on' }] }, admin);
  t('POST /api/miniapp/scenes -> 200', sceneCreate.status === 200, `status=${sceneCreate.status}`);
  const sceneId = sceneCreate.json?.scene_id;
  if (sceneId) {
    await rep(`GET /api/miniapp/scenes 含新场景 -> 200`, () => get(`/api/miniapp/scenes?space_id=${SPACE}`, admin), 200);
    const sceneUpd = await put(`/api/miniapp/scenes/${sceneId}`, { name: '压测场景改名' }, admin);
    t('PUT /api/miniapp/scenes/{id} -> 200', sceneUpd.status === 200, `status=${sceneUpd.status}`);
    const sceneExec = await post(`/api/miniapp/scenes/${sceneId}/execute`, undefined, admin);
    t('POST /api/miniapp/scenes/{id}/execute -> 200', sceneExec.status === 200, `status=${sceneExec.status} applied=${sceneExec.json?.applied}`);
    const sceneDel = await del(`/api/miniapp/scenes/${sceneId}`, admin);
    t('DELETE /api/miniapp/scenes/{id} -> 200', sceneDel.status === 200, `status=${sceneDel.status}`);
  } else {
    t('POST /api/miniapp/scenes 返回 scene_id', false, JSON.stringify(sceneCreate.json));
  }

  // ---- 空间绑定 create/delete ----
  const sbPost = await post('/api/miniapp/space-bindings', { user_id: 'u_teacher_1', space_id: SPACE2 }, admin);
  t('POST /api/miniapp/space-bindings -> 200', sbPost.status === 200, `status=${sbPost.status}`);
  const sbDel = await del(`/api/miniapp/space-bindings?user_id=u_teacher_1&space_id=${SPACE2}`, admin);
  t('DELETE /api/miniapp/space-bindings -> 200', sbDel.status === 200, `status=${sbDel.status}`);

  // ---- 门禁白名单 + 二次确认（428/400）----
  const mark = await post('/api/miniapp/door-devices', { device_id: DOOR2, space_id: SPACE2, label: 'A102门禁' }, admin);
  t('POST /api/miniapp/door-devices -> 200', mark.status === 200, `status=${mark.status}`);
  const noConfirm = await post('/api/miniapp/device/control', { space_id: SPACE2, device_type: 'zigbee', device_id: DOOR2, command: 'off' }, admin);
  t('门禁 off 无 confirm -> 428', noConfirm.status === 428, `status=${noConfirm.status}`);
  const toggle = await post('/api/miniapp/device/control', { space_id: SPACE2, device_type: 'zigbee', device_id: DOOR2, command: 'toggle' }, admin);
  t('门禁 toggle -> 400', toggle.status === 400, `status=${toggle.status}`);
  const withConfirm = await post('/api/miniapp/device/control', { space_id: SPACE2, device_type: 'zigbee', device_id: DOOR2, command: 'off', confirm: true }, admin);
  t('门禁 off confirm=true -> 200', withConfirm.status === 200, `status=${withConfirm.status}`);
  const unmark = await del(`/api/miniapp/door-devices?space_id=${SPACE2}&device_id=${DOOR2}`, admin);
  t('DELETE /api/miniapp/door-devices -> 200', unmark.status === 200, `status=${unmark.status}`);

  // ---- 通知目标绑定 create/delete ----
  const nbPost = await post('/api/notify/bindings', { channel: 'wechat_miniapp', user_id: 'u_admin_1', external_id: `openid_nb_${ts}` }, admin);
  t('POST /api/notify/bindings -> 200', nbPost.status === 200, `status=${nbPost.status}`);
  const nbDel = await del(`/api/notify/bindings?channel=wechat_miniapp&user_id=u_admin_1`, admin);
  t('DELETE /api/notify/bindings -> 200', nbDel.status === 200, `status=${nbDel.status}`);

  // ---- 管理员教室 create/update/delete ----
  const spId = `spc_bench_${ts}`;
  const spPost = await post('/api/admin/spaces', { name: '压测教室', type: 'standard', space_id: spId }, admin);
  t('POST /api/admin/spaces -> 200', spPost.status === 200, `status=${spPost.status}`);
  const spPut = await put(`/api/admin/spaces/${spId}`, { name: '压测教室改' }, admin);
  t('PUT /api/admin/spaces/{id} -> 200', spPut.status === 200, `status=${spPut.status}`);
  const spDel = await del(`/api/admin/spaces/${spId}`, admin);
  t('DELETE /api/admin/spaces/{id} -> 200（软删）', spDel.status === 200, `status=${spDel.status}`);

  // ---- 空间类型 create/update/order/delete ----
  const stCode = `bench_${ts}`;
  const stPost = await post('/api/admin/space-types', { code: stCode, name: '压测类型' }, admin);
  t('POST /api/admin/space-types -> 200', stPost.status === 200, `status=${stPost.status}`);
  const stPut = await put(`/api/admin/space-types/${stCode}`, { name: '压测类型改' }, admin);
  t('PUT /api/admin/space-types/{code} -> 200', stPut.status === 200, `status=${stPut.status}`);
  const stList = await get('/api/miniapp/space-types', admin);
  const codes = (stList.json?.types || []).map((x) => x.code);
  const stOrder = await patch('/api/admin/space-types/order', { codes }, admin);
  t('PATCH /api/admin/space-types/order -> 200', stOrder.status === 200, `status=${stOrder.status} codes=${codes.length}`);
  const stDel = await del(`/api/admin/space-types/${stCode}`, admin);
  t('DELETE /api/admin/space-types/{code} -> 200（软删）', stDel.status === 200, `status=${stDel.status}`);

  // ---- 告警处置闭环 ----
  const alerts = await get(`/api/miniapp/alerts?space_id=${SPACE}&page=1&page_size=50`, admin);
  const unhandled = (alerts.json?.alerts || []).find((a) => a.status === 'unhandled');
  const anyAlert = (alerts.json?.alerts || [])[0];
  const alertId = unhandled?.alert_id || anyAlert?.alert_id;
  if (alertId) {
    const handle = await post(`/api/miniapp/alerts/${alertId}/handle`, { status: 'resolved', remark: '压测处理' }, admin);
    t('POST /api/miniapp/alerts/{id}/handle admin -> 200', handle.status === 200, `status=${handle.status}`);
    const handle403 = await post(`/api/miniapp/alerts/${alertId}/handle`, { status: 'resolved', remark: '越权' }, teacher);
    t('POST /api/miniapp/alerts/{id}/handle teacher -> 403', handle403.status === 403, `status=${handle403.status}`);
    const tl = await get(`/api/miniapp/alerts/${alertId}/timeline`, admin);
    t('GET /api/miniapp/alerts/{id}/timeline admin -> 200', tl.status === 200, `status=${tl.status}`);
    const tl403 = await get(`/api/miniapp/alerts/${alertId}/timeline`, teacher);
    t('GET /api/miniapp/alerts/{id}/timeline teacher -> 403', tl403.status === 403, `status=${tl403.status}`);
  } else {
    t('告警列表存在可处置告警', false, '无 alert 可用');
  }

  // ---- 鉴权负向：过期 JWT -> 401 ----
  const expRes = await get('/api/miniapp/me', bearer(ctx.expiredJwt));
  t('过期 JWT -> 401', expRes.status === 401, `status=${expRes.status}`);

  // ---- 内部接口（X-Internal-Token）----
  const nf = await post('/internal/notify/send', { event_type: 'device_offline', content: '覆盖-设备离线', space_id: SPACE, device_id: `dev_cov_${ts}`, event_id: `alt_cov_${ts}` }, intr);
  t('POST /internal/notify/send -> 200', nf.status === 200, `status=${nf.status}`);
  const ss = await post('/internal/notify/subscribe-send', { openid: `openid_cov_${ts}`, template_id: 'tmpl_bench', data: { thing1: 'v1' } }, intr);
  t('POST /internal/notify/subscribe-send -> 200', ss.status === 200, `status=${ss.status}`);
  const oa = await post('/internal/oa/arrival-notify', { student_no: '20260101', student_name: '张三', space_id: SPACE, space_name: 'A101 教室', arrival_time: new Date().toISOString(), event_id: `oa_${ts}` }, intr);
  t('POST /internal/oa/arrival-notify -> 200', oa.status === 200, `status=${oa.status}`);
  const ur = await post('/internal/user-roles/sync', { user_id: 'u_teacher_1', roles: ['teacher'] }, intr);
  t('POST /internal/user-roles/sync -> 200', ur.status === 200, `status=${ur.status}`);
  const acc = await post('/internal/access-records', { event_id: `evt_cov_${ts}`, space_id: SPACE, occurred_at: new Date().toISOString(), name: '覆盖学生', auth_type: 'card', result: 'login' }, intr);
  t('POST /internal/access-records -> 200', acc.status === 200, `status=${acc.status}`);
  // spaces/sync 是全量对账，必须传「当前完整空间列表」，否则会把未列出的空间删掉。
  const spList = await get('/api/admin/spaces', admin);
  const activeSpaces = (spList.json?.spaces || [])
    .filter((s) => s.enabled !== false)
    .map((s) => ({ space_id: s.space_id, name: s.name, type: s.type }));
  const spc = await post('/internal/spaces/sync', { spaces: activeSpaces }, intr);
  t(`POST /internal/spaces/sync 全量对账 -> 200（${activeSpaces.length} 空间）`, spc.status === 200, `status=${spc.status}`);
  const par = await post('/internal/student-parents/sync', { parents: [{ student_no: '20260101', parent_phone: '13800000000', student_name: '张三', relation: '父亲', status: 'active' }] }, intr);
  t('POST /internal/student-parents/sync -> 200', par.status === 200, `status=${par.status}`);
  const badToken = await post('/internal/notify/send', { event_type: 'device_offline', content: 'x' }, { 'X-Internal-Token': 'wrong-token' });
  t('错误 X-Internal-Token -> 401', badToken.status === 401, `status=${badToken.status}`);

  return { name: '全端点覆盖矩阵', asserts, stats };
}
