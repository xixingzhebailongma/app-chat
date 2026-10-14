/**
 * Mock 演示数据：USE_MOCK=true 时替代真实网络请求，可完整跑通全部界面与交互。
 * - 模拟云端接口延迟与返回结构 { data } / { error }
 * - 模拟 WebSocket：30s 心跳、传感器数据漂移 device_update、设备指令 cmd_response
 * 演示账号：teacher / 123456（教师），admin / 123456（管理员）
 */
import { store } from '../store'

const delay = (ms) => new Promise((r) => setTimeout(r, ms))
const ok = (data) => delay(90 + Math.random() * 120).then(() => ({ data }))
const fail = (message, code = 400) => delay(120).then(() => ({ error: { code, message } }))
const iso = (offsetMin = 0) => new Date(Date.now() - offsetMin * 60000).toISOString()

/* ---------- 教室目录 ---------- */
const spaces = [
  { space_id: 'space-101', name: '101 智慧教室' },
  { space_id: 'space-201', name: '201 多媒体教室' },
  { space_id: 'space-301', name: '物理实验室' },
]

/* ---------- 设备库（按 space_id 分组，结构与边侧 /api/devices 一致） ---------- */
const devices = {
  'space-101': [
    { device_id: 'ZB_00112233AA01', device_type: 'zigbee', zb_type: 'switch', label: '教室主照明', status: 'online', app: 'on', gateway_id: 'GW-01', space_id: 'space-101', last_seen: iso(1) },
    { device_id: 'ZB_00112233AA02', device_type: 'zigbee', zb_type: 'switch', label: '黑板灯组', status: 'online', app: 'off', gateway_id: 'GW-01', space_id: 'space-101', last_seen: iso(1) },
    { device_id: 'ZB_00112233AA03', device_type: 'zigbee', zb_type: 'switch', label: '空调', status: 'online', app: 'on', gateway_id: 'GW-01', space_id: 'space-101', last_seen: iso(2) },
    { device_id: 'ZB_00112233AA04', device_type: 'zigbee', zb_type: 'switch', label: '智能门锁', status: 'online', app: 'off', gateway_id: 'GW-01', space_id: 'space-101', last_seen: iso(1) },
    { device_id: 'ZB_00112233AA05', device_type: 'zigbee', zb_type: 'switch', label: '投影幕布', status: 'offline', app: 'off', gateway_id: 'GW-01', space_id: 'space-101', last_seen: iso(35) },
    { device_id: 'ZB_sensor_01', device_type: 'zigbee', zb_type: 'sensor', label: '温湿度传感器', status: 'online', app: JSON.stringify({ temperature: 24.6, humidity: 57 }), gateway_id: 'GW-01', space_id: 'space-101', last_seen: iso(0.2) },
    { device_id: 'ZB_sensor_02', device_type: 'zigbee', zb_type: 'sensor', label: '空气质量传感器', status: 'online', app: JSON.stringify({ co2: 860, pm25: 42 }), gateway_id: 'GW-01', space_id: 'space-101', last_seen: iso(0.5) },
    { device_id: 'ZB_sensor_03', device_type: 'zigbee', zb_type: 'sensor', label: '光照传感器', status: 'online', app: JSON.stringify({ light: 452 }), gateway_id: 'GW-01', space_id: 'space-101', last_seen: iso(1) },
    { device_id: 'ZB_sensor_04', device_type: 'zigbee', zb_type: 'sensor', label: '人体感应', status: 'online', app: JSON.stringify({ pir: true }), gateway_id: 'GW-01', space_id: 'space-101', last_seen: iso(0.1) },
    { device_id: 'FUHE_SCREEN_101', device_type: 'fuhe-screen', zb_type: '', label: '综合屏', status: 'online', app: 'on', info: JSON.stringify({ cpu: 32, mem: 48, uptime: '3天2小时' }), space_id: 'space-101', last_seen: iso(0.3) },
    { device_id: 'FUHE_ENC_101', device_type: 'fuhe-encoder', zb_type: '', label: '编解码器', status: 'online', app: 'off', info: JSON.stringify({ cpu: 18, mem: 35, uptime: '12天' }), space_id: 'space-101', last_seen: iso(3) },
  ],
  'space-201': [
    { device_id: 'ZB_201A_01', device_type: 'zigbee', zb_type: 'switch', label: '教室主照明', status: 'online', app: 'on', gateway_id: 'GW-02', space_id: 'space-201', last_seen: iso(1) },
    { device_id: 'ZB_201A_02', device_type: 'zigbee', zb_type: 'switch', label: '空调', status: 'online', app: 'off', gateway_id: 'GW-02', space_id: 'space-201', last_seen: iso(2) },
    { device_id: 'ZB_201S_01', device_type: 'zigbee', zb_type: 'sensor', label: '温湿度传感器', status: 'online', app: JSON.stringify({ temperature: 26.2, humidity: 61 }), gateway_id: 'GW-02', space_id: 'space-201', last_seen: iso(0.4) },
    { device_id: 'ZB_201S_02', device_type: 'zigbee', zb_type: 'sensor', label: 'CO₂传感器', status: 'online', app: JSON.stringify({ co2: 980 }), gateway_id: 'GW-02', space_id: 'space-201', last_seen: iso(0.8) },
  ],
  'space-301': [
    { device_id: 'ZB_301A_01', device_type: 'zigbee', zb_type: 'switch', label: '实验室照明', status: 'online', app: 'off', gateway_id: 'GW-03', space_id: 'space-301', last_seen: iso(1) },
    { device_id: 'ZB_301S_01', device_type: 'zigbee', zb_type: 'sensor', label: '温湿度传感器', status: 'offline', app: JSON.stringify({ temperature: 25.0, humidity: 55 }), gateway_id: 'GW-03', space_id: 'space-301', last_seen: iso(42) },
  ],
}

function allDevices() {
  return Object.keys(devices).reduce((acc, k) => acc.concat(devices[k]), [])
}
function findDevice(id) {
  return allDevices().find((d) => d.device_id === id)
}

/* ---------- 告警库 ---------- */
let alarmSeq = 0
const alarms = [
  {
    id: 'a1', space_id: 'space-101', space_name: '101 智慧教室', type: 'sensor_threshold', level: 'critical',
    title: '温度超过阈值', summary: '温度 31.2°C，已持续 6 分钟', occurred_at: iso(12), status: 'pending',
    timeline: [
      { stage: '发现', time: iso(18), note: '温度首次超过 30°C 阈值' },
      { stage: '推送', time: iso(12), note: '持续超阈值 5 分钟以上，已推送订阅消息' },
    ],
  },
  {
    id: 'a2', space_id: 'space-101', space_name: '101 智慧教室', type: 'sensor_threshold', level: 'warning',
    title: 'CO₂ 浓度偏高', summary: 'CO₂ 1120ppm，建议通风', occurred_at: iso(40), status: 'processing',
    suppress_reason: '同设备同类告警处于 30 分钟冷却期，已合并', handled_by: '王管理', handled_at: iso(20),
    timeline: [
      { stage: '发现', time: iso(45), note: 'CO₂ 首次超过 1000ppm' },
      { stage: '推送', time: iso(40), note: '已推送订阅消息' },
      { stage: '处理', time: iso(20), note: '管理员已查看，安排通风处理', by: '王管理' },
    ],
  },
  {
    id: 'a3', space_id: 'space-201', space_name: '201 多媒体教室', type: 'device_offline', level: 'info',
    title: '温湿度传感器离线', summary: '传感器已离线 35 分钟', occurred_at: iso(75), status: 'resolved',
    suppress_reason: '短暂离线（未满 30 分钟）的抖动未推送',
    handled_by: '王管理', handled_at: iso(60),
    timeline: [
      { stage: '发现', time: iso(75), note: '传感器心跳中断' },
      { stage: '推送', time: iso(40), note: '离线超 30 分钟，推送管理员' },
      { stage: '处理', time: iso(60), note: '现场排查，网关断电导致，已恢复', by: '王管理' },
    ],
  },
  {
    id: 'a4', space_id: 'space-101', space_name: '101 智慧教室', type: 'security', level: 'critical',
    title: '非授课时段有人进入', summary: '人脸识别：检测到未登记面孔', occurred_at: iso(120), status: 'pending',
    degrade_note: '推送主渠道失败，已降级为短信通知',
    timeline: [
      { stage: '发现', time: iso(120), note: '人体感应触发 + 人脸比对未通过' },
      { stage: '推送', time: iso(119), note: '订阅消息主渠道失败，已降级短信' },
    ],
  },
  {
    id: 'a5', space_id: 'space-101', space_name: '101 智慧教室', type: 'device_offline', level: 'warning',
    title: '投影幕布离线', summary: '投影幕布控制器离线', occurred_at: iso(200), status: 'resolved',
    handled_by: '王管理', handled_at: iso(150),
    timeline: [
      { stage: '发现', time: iso(230), note: '设备心跳中断' },
      { stage: '推送', time: iso(200), note: '已推送管理员' },
      { stage: '处理', time: iso(150), note: '重新上电后恢复', by: '王管理' },
    ],
  },
  {
    id: 'a6', space_id: 'space-301', space_name: '物理实验室', type: 'sensor_threshold', level: 'warning',
    title: '湿度偏低', summary: '湿度 26%，低于下限 30%', occurred_at: iso(60 * 26), status: 'resolved',
    handled_by: '王管理', handled_at: iso(60 * 25),
    timeline: [
      { stage: '发现', time: iso(60 * 31), note: '湿度持续走低' },
      { stage: '推送', time: iso(60 * 26), note: '持续 5 分钟以上，已推送' },
      { stage: '处理', time: iso(60 * 25), note: '开启加湿器', by: '王管理' },
    ],
  },
  {
    id: 'a7', space_id: 'space-201', space_name: '201 多媒体教室', type: 'security', level: 'critical',
    title: '门磁异常', summary: '授课时段外门锁被开启', occurred_at: iso(60 * 5), status: 'pending',
    timeline: [
      { stage: '发现', time: iso(60 * 5), note: '门磁传感器触发' },
      { stage: '推送', time: iso(60 * 5 - 1), note: '已推送订阅消息' },
    ],
  },
  {
    id: 'a8', space_id: 'space-301', space_name: '物理实验室', type: 'system', level: 'info',
    title: '边侧主机重启', summary: '系统更新后自动重启', occurred_at: iso(60 * 49), status: 'resolved',
    suppress_reason: '系统例行事件，仅记录不推送',
    handled_by: '王管理', handled_at: iso(60 * 48),
    timeline: [
      { stage: '发现', time: iso(60 * 49), note: 'agent_event: host reboot' },
      { stage: '处理', time: iso(60 * 48), note: '确认例行更新', by: '王管理' },
    ],
  },
  {
    id: 'a9', space_id: 'space-101', space_name: '101 智慧教室', type: 'sensor_threshold', level: 'warning',
    title: 'PM2.5 偏高', summary: 'PM2.5 82μg/m³', occurred_at: iso(60 * 27), status: 'resolved',
    suppress_reason: '30 分钟冷却期内已合并（前序同类告警未处理）',
    handled_by: '王管理', handled_at: iso(60 * 26),
    timeline: [
      { stage: '发现', time: iso(60 * 32), note: 'PM2.5 超过 75μg/m³' },
      { stage: '推送', time: iso(60 * 27), note: '已推送订阅消息' },
      { stage: '处理', time: iso(60 * 26), note: '开启新风 30 分钟', by: '王管理' },
    ],
  },
]

/* ---------- 今日进出记录 ---------- */
const sessions = [
  { name: '张老师', auth_type: 'face', result: 'login', time: iso(8), space_id: 'space-101' },
  { name: '李明同学', auth_type: 'card', result: 'login', time: iso(24), space_id: 'space-101' },
  { name: '赵倩同学', auth_type: 'card', result: 'login', time: iso(38), space_id: 'space-101' },
  { name: '未登记人员', auth_type: 'face', result: 'denied', time: iso(52), space_id: 'space-101' },
  { name: '张老师', auth_type: 'face', result: 'login', time: iso(66), space_id: 'space-101' },
  { name: '王浩同学', auth_type: 'card', result: 'login', time: iso(95), space_id: 'space-101' },
]

/* ---------- 课表 ---------- */
const schedules = {
  'space-101': {
    space_id: 'space-101', updated_at: iso(60 * 30),
    entries: [
      { weekday: 1, period: 1, subject: '高等数学', teacher: '张老师', start: '08:00', end: '08:45' },
      { weekday: 1, period: 2, subject: '大学英语', teacher: '刘老师', start: '08:55', end: '09:40' },
      { weekday: 1, period: 3, subject: '数据结构', teacher: '张老师', start: '10:10', end: '10:55' },
      { weekday: 1, period: 4, subject: '体育', teacher: '陈老师', start: '14:00', end: '14:45' },
    ],
  },
  'space-201': {
    space_id: 'space-201', updated_at: iso(60 * 50),
    entries: [
      { weekday: 1, period: 1, subject: '大学物理', teacher: '周老师', start: '08:00', end: '08:45' },
      { weekday: 1, period: 2, subject: '电路分析', teacher: '吴老师', start: '08:55', end: '09:40' },
    ],
  },
  'space-301': { space_id: 'space-301', entries: [] },
}

/* ---------- 用户与教室绑定 ---------- */
let users = [
  { user_id: 'u1', username: 'zhang', name: '张老师', role: 'teacher', spaces: ['space-101'] },
  { user_id: 'u2', username: 'li', name: '李老师', role: 'teacher', spaces: ['space-201'] },
  { user_id: 'u3', username: 'wang', name: '王管理', role: 'admin', spaces: [] },
]

/* ================= Mock 接口 ================= */

export function login(username, password) {
  if (username === 'teacher' && password === '123456') {
    return ok({
      token: 'mock-token-teacher-' + Date.now(),
      user: { user_id: 'u1', username: 'teacher', name: '张老师', role: 'teacher' },
      space_id: 'space-101',
      spaces,
    })
  }
  if (username === 'admin' && password === '123456') {
    return ok({
      token: 'mock-token-admin-' + Date.now(),
      user: { user_id: 'u3', username: 'admin', name: '王管理', role: 'admin' },
      space_id: '',
      spaces,
    })
  }
  return fail('账号或密码错误', 401)
}

export function getDevices(spaceId) {
  return ok((devices[spaceId] || []).map((d) => Object.assign({}, d)))
}

export function getSchedules(spaceId) {
  const s = schedules[spaceId]
  return ok(s ? Object.assign({}, s, { entries: s.entries.map((e) => Object.assign({}, e)) }) : { space_id: spaceId, entries: [] })
}

export function getFaceSessions(params = {}) {
  let list = sessions.filter((s) => !params.space_id || s.space_id === params.space_id)
  list = list.slice().sort((a, b) => new Date(b.time) - new Date(a.time))
  return ok(list.map((s) => Object.assign({}, s)))
}

export function getSipEndpoints() {
  return ok({
    center: { ext: '8000', name: '控制中心', ip: '10.1.0.10', sip_status: 'online', space_id: '' },
    classrooms: spaces.map((s, i) => Object.assign({}, s, {
      type: 'classroom', ext: '80' + (i + 1), ip: '10.1.1.' + (i + 1),
      sip_status: i === 1 ? 'offline' : 'online', last_seen: iso(1 + i),
    })),
  })
}

export function getOverview() {
  const rows = spaces.map((s) => {
    const ds = devices[s.space_id] || []
    const online = ds.filter((d) => d.status === 'online').length
    const pending = alarms.filter((a) => a.space_id === s.space_id && a.status === 'pending').length
    return { space_id: s.space_id, name: s.name, device_total: ds.length, device_online: online, pending_alarms: pending }
  })
  return ok({
    classrooms: rows,
    totals: {
      spaces: rows.length,
      devices_online: rows.reduce((a, r) => a + r.device_online, 0),
      devices_total: rows.reduce((a, r) => a + r.device_total, 0),
      pending_alarms: rows.reduce((a, r) => a + r.pending_alarms, 0),
    },
  })
}

export function getAlarms(params = {}) {
  let list = alarms.slice()
  if (params.space_id) list = list.filter((a) => a.space_id === params.space_id)
  if (params.type) list = list.filter((a) => a.type === params.type)
  if (params.status) {
    const set = String(params.status).split(',')
    list = list.filter((a) => set.includes(a.status))
  }
  if (params.start) list = list.filter((a) => new Date(a.occurred_at) >= new Date(params.start))
  if (params.end) list = list.filter((a) => new Date(a.occurred_at) <= new Date(params.end))
  list.sort((a, b) => new Date(b.occurred_at) - new Date(a.occurred_at))
  const page = params.page || 1
  const size = params.page_size || 8
  return ok({
    list: list.slice((page - 1) * size, page * size).map((a) => JSON.parse(JSON.stringify(a))),
    total: list.length,
    has_more: page * size < list.length,
  })
}

export function getAlarmDetail(id) {
  const a = alarms.find((x) => x.id === id)
  return a ? ok(JSON.parse(JSON.stringify(a))) : fail('告警不存在', 404)
}

export function handleAlarm(id, payload = {}) {
  const a = alarms.find((x) => x.id === id)
  if (!a) return fail('告警不存在', 404)
  const now = new Date().toISOString()
  if (payload.status) {
    a.status = payload.status
    a.handled_by = (store.user && store.user.name) || '当前用户'
    a.handled_at = now
  }
  const statusText = payload.status === 'resolved' ? '标记为已解决' : payload.status === 'processing' ? '标记为处理中' : '添加备注'
  a.timeline = a.timeline || []
  a.timeline.push({ stage: '处理', time: now, note: payload.note || statusText, by: (store.user && store.user.name) || '当前用户' })
  return ok(JSON.parse(JSON.stringify(a)))
}

export function getUsers() {
  return ok(users.map((u) => Object.assign({}, u, { space_names: (u.spaces || []).map((id) => (spaces.find((s) => s.space_id === id) || {}).name).filter(Boolean) })))
}

export function bindUserSpace(userId, spaceId) {
  const u = users.find((x) => x.user_id === userId)
  if (!u) return fail('用户不存在', 404)
  if (!u.spaces) u.spaces = []
  if (u.spaces.includes(spaceId)) return fail('该教室已绑定', 400)
  u.spaces.push(spaceId)
  return ok({ user_id: userId, spaces: u.spaces.slice() })
}

export function unbindUserSpace(userId, spaceId) {
  const u = users.find((x) => x.user_id === userId)
  if (!u) return fail('用户不存在', 404)
  u.spaces = (u.spaces || []).filter((s) => s !== spaceId)
  return ok({ user_id: userId, spaces: u.spaces.slice() })
}

export function rebootDevice(deviceId) {
  const d = findDevice(deviceId)
  if (!d) return fail('设备不存在', 404)
  return ok({ status: 'ok', device_id: deviceId, reboot: true })
}

/* ================= Mock WebSocket ================= */

let activeWs = null    // 当前 mock 连接实例
let mockOnMessage = null

export function createMockWs(url, onMessage, onOpen, onClose) {
  if (activeWs) { try { activeWs.destroy() } catch (e) { /* noop */ } }
  const inst = {
    destroyed: false,
    timers: [],
    close() { this.destroy() },
    destroy() {
      this.destroyed = true
      this.timers.forEach((t) => clearInterval(t))
      if (activeWs === this) { activeWs = null; mockOnMessage = null }
    },
  }
  activeWs = inst
  mockOnMessage = onMessage
  const spaceMatch = url.match(/space_id=([^&]*)/)
  const spaceId = spaceMatch ? decodeURIComponent(spaceMatch[1]) : ''

  setTimeout(() => { if (!inst.destroyed) onOpen() }, 200)

  // 服务端心跳（每 30s，客户端忽略）
  inst.timers.push(setInterval(() => { if (!inst.destroyed) onMessage({ type: 'ping' }) }, 30000))

  // 传感器数据漂移 → device_update（仅订阅的空间，服务端已过滤）
  inst.timers.push(setInterval(() => {
    if (inst.destroyed || !spaceId) return
    const sensors = (devices[spaceId] || []).filter((d) => d.zb_type === 'sensor' && d.status === 'online')
    if (!sensors.length) return
    const d = sensors[Math.floor(Math.random() * sensors.length)]
    let app
    try { app = JSON.parse(d.app) } catch (e) { return }
    if (app.temperature !== undefined) app.temperature = Math.round((app.temperature + (Math.random() - 0.5) * 0.4) * 10) / 10
    if (app.humidity !== undefined) app.humidity = Math.max(20, Math.min(80, app.humidity + Math.round((Math.random() - 0.5) * 2)))
    if (app.co2 !== undefined) app.co2 = Math.max(400, Math.min(1600, app.co2 + Math.round((Math.random() - 0.5) * 60)))
    if (app.light !== undefined) app.light = Math.max(100, Math.min(900, app.light + Math.round((Math.random() - 0.5) * 40)))
    d.app = JSON.stringify(app)
    d.last_seen = new Date().toISOString()
    onMessage({ type: 'device_update', payload: Object.assign({}, d) })
  }, 8000))

  return inst
}

/** 模拟设备指令：HTTP 200 仅代表已下发 MQTT，真实结果由 cmd_response / device_update 确认 */
export function sendDeviceCmd(deviceType, deviceId, command) {
  if (!['on', 'off', 'toggle'].includes(command)) return fail('command 不合法', 400)
  const d = findDevice(deviceId)
  if (!d) return fail('设备不存在', 404)
  if (d.device_type === 'zigbee' && !d.gateway_id) return fail('设备未绑定网关', 400)

  if (d.status !== 'online') {
    // 设备离线：指令可下发但永远不会有回执 → 前端三段式超时兜底会触发
    return ok({ status: 'ok', topic: 'mock/topic/' + deviceId })
  }

  setTimeout(() => {
    if (mockOnMessage && activeWs && !activeWs.destroyed) {
      mockOnMessage({ type: 'cmd_response', payload: { device_id: deviceId, device_type: d.device_type, status: 'ok', command } })
    }
  }, 500)
  setTimeout(() => {
    if (mockOnMessage && activeWs && !activeWs.destroyed) {
      d.app = command === 'toggle' ? (d.app === 'on' ? 'off' : 'on') : command
      d.last_seen = new Date().toISOString()
      mockOnMessage({ type: 'device_update', payload: Object.assign({}, d) })
    }
  }, 950)
  return ok({ status: 'ok', topic: 'mock/topic/' + deviceId })
}
