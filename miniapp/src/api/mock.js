// 本地 mock：镜像 wechat-gateway 的 dev seed 数据与 mock-go-backend 的返回结构，
// 并复刻后端的关键语义（空间越权、教师脱敏、高危指令确认、仅管理员可处理告警）。
// 用途：不启动后端、不接微信授权，就能在浏览器/开发者工具里预览整套界面。
import { store, isLoggedIn } from '../store/index'

// 空间：带类型（standard 标准教室 / lecture 多媒体报告厅 / office 办公室）。
// 第一级按类型区分，第二级才是类型下的各个教室。
const spaces = [
  { space_id: 'spc_a8acdd5c', name: 'A101 教室', type: 'standard', enabled: true, source: 'edge' },
  { space_id: 'spc_std_a102', name: 'A102 教室', type: 'standard', enabled: true, source: 'edge' },
  { space_id: 'spc_b7bee44d', name: '多媒体报告厅', type: 'lecture', enabled: true, source: 'edge' },
  { space_id: 'spc_office_1', name: '教师办公室', type: 'office', enabled: true, source: 'edge' },
  { space_id: 'spc_gym_1', name: '体育馆', type: 'gym', enabled: true, source: 'edge' },
  { space_id: 'spc_lab_1', name: '化学实验室', type: 'lab', enabled: true, source: 'edge' },
  { space_id: 'spc_lib_1', name: '图书馆', type: 'library', enabled: true, source: 'edge' },
  { space_id: 'spc_canteen_1', name: '学生食堂', type: 'canteen', enabled: true, source: 'edge' },
]

// 设备：按空间组织，设备类型按空间类型各有所属
// （标准教室：照明/电子班牌/综合屏/门禁；报告厅：主屏/照明/门禁/传感器；办公室：照明/门禁/传感器）。
const devices = {
  spc_a8acdd5c: [
    { device_id: 'dev_c2936745', label: '教室照明', type: 'light', online: true },
    { device_id: 'dev_1a2b3c4d', label: '电子班牌', type: 'sign', online: true },
    { device_id: 'dev_9f8e7d6c', label: '综合屏', type: 'screen', online: false },
    { device_id: 'dev_d0or_a101', label: '前门门禁', type: 'door', online: true },
    { device_id: 'dev_sens_a101', label: '环境传感器', type: 'sensor', online: true },
  ],
  spc_std_a102: [
    { device_id: 'dev_aa11bb22', label: '教室照明', type: 'light', online: true },
    { device_id: 'dev_cc33dd44', label: '电子班牌', type: 'sign', online: false },
    { device_id: 'dev_ee55ff66', label: '综合屏', type: 'screen', online: true },
    { device_id: 'dev_d0or_a102', label: '前门门禁', type: 'door', online: true },
  ],
  spc_b7bee44d: [
    { device_id: 'dev_55aa66bb', label: '报告厅主屏', type: 'screen', online: true },
    { device_id: 'dev_77cc88dd', label: '报告厅照明', type: 'light', online: true },
    { device_id: 'dev_99aabbcc', label: '后门门禁', type: 'door', online: false },
    { device_id: 'dev_sens_lect', label: '环境传感器', type: 'sensor', online: true },
  ],
  spc_office_1: [
    { device_id: 'dev_off_light', label: '办公区照明', type: 'light', online: true },
    { device_id: 'dev_off_door', label: '办公室门禁', type: 'door', online: false },
    { device_id: 'dev_off_sens', label: '温湿度传感器', type: 'sensor', online: false },
  ],
  spc_gym_1: [
    { device_id: 'dev_gym_light', label: '场馆照明', type: 'light', online: true },
    { device_id: 'dev_gym_screen', label: '大屏', type: 'screen', online: true },
    { device_id: 'dev_gym_door', label: '入口门禁', type: 'door', online: true },
    { device_id: 'dev_gym_sens', label: '环境传感器', type: 'sensor', online: false },
  ],
  spc_lab_1: [
    { device_id: 'dev_lab_light', label: '实验区照明', type: 'light', online: true },
    { device_id: 'dev_lab_screen', label: '实验演示屏', type: 'screen', online: true },
    { device_id: 'dev_lab_door', label: '实验室门禁', type: 'door', online: true },
    { device_id: 'dev_lab_sens', label: '通风传感器', type: 'sensor', online: false },
  ],
  spc_lib_1: [
    { device_id: 'dev_lib_light', label: '阅览区照明', type: 'light', online: true },
    { device_id: 'dev_lib_door', label: '图书馆门禁', type: 'door', online: true },
    { device_id: 'dev_lib_sens', label: '环境传感器', type: 'sensor', online: true },
  ],
  spc_canteen_1: [
    { device_id: 'dev_ctn_light', label: '餐厅照明', type: 'light', online: true },
    { device_id: 'dev_ctn_screen', label: '窗口大屏', type: 'screen', online: true },
    { device_id: 'dev_ctn_door', label: '入口门禁', type: 'door', online: true },
    { device_id: 'dev_ctn_sens', label: '温湿度传感器', type: 'sensor', online: false },
  ],
}

// 设备开关态（与"在线/离线"连通性区分）：device_id -> on/off。executeScene/control 更新。
const switchStates = {}

// 把旧的 {type, online} 设备数据转成「综合屏 API 对接文档」的字段模型：
// device_type / status("online"|"offline") / app / zb_type / gateway_id / info。
function toDeviceDoc(dev, spaceId) {
  const online = dev.online
  const t = dev.type
  const d = {
    device_id: dev.device_id,
    device_type: t,
    status: online ? 'online' : 'offline',
    app: switchStates[dev.device_id] || 'off',
    label: dev.label,
    space_id: spaceId,
    last_seen: '2026-09-11T04:52:49Z',
  }
  if (t === 'sensor') {
    d.device_type = 'zigbee'
    d.zb_type = 'sensor'
    d.gateway_id = 'dev_gw_' + spaceId
    d.app = JSON.stringify({ temperature: 25.1, humidity: 60, co2: 800 })
  } else if (t === 'light' || t === 'door') {
    d.device_type = 'zigbee'
    d.zb_type = 'switch'
    // zb_role 仅用于 UI 展示；场景匹配在后端按 label 兜底，前端不参与场景匹配逻辑。
    d.zb_role = t
    d.gateway_id = 'dev_gw_' + spaceId
  } else if (t === 'sign') {
    d.device_type = 'fuhe-board'
    d.info = '{}'
  } else if (t === 'screen') {
    d.device_type = 'fuhe-screen'
    d.info = '{}'
  }
  return d
}

// 告警 mock 的 created_at 用 ISO-8601 字符串，与后端 TIMESTAMPTZ 返回对齐。
const agoIso = (ms) => new Date(Date.now() - ms).toISOString()

const alerts = [
  {
    alert_id: 'alt_1',
    space_id: 'spc_a8acdd5c',
    event_type: 'device_offline',
    status: 'unhandled',
    operator_id: '',
    remark: '',
    device_id: 'dev_9f8e7d6c',
    device_label: '综合屏',
    created_at: agoIso(1000 * 60 * 8),
  },
  {
    alert_id: 'alt_2',
    space_id: 'spc_b7bee44d',
    event_type: 'sensor_threshold',
    status: 'unhandled',
    operator_id: '',
    remark: '',
    device_id: '',
    device_label: '',
    created_at: agoIso(1000 * 60 * 25),
  },
  {
    alert_id: 'alt_3',
    space_id: 'spc_a8acdd5c',
    event_type: 'face_login_failed',
    status: 'resolved',
    operator_id: 'u_admin_1',
    remark: '已处理',
    device_id: '',
    device_label: '',
    created_at: agoIso(1000 * 60 * 60 * 2),
  },
  {
    alert_id: 'alt_4',
    space_id: 'spc_office_1',
    event_type: 'device_offline',
    status: 'unhandled',
    operator_id: '',
    remark: '',
    device_id: 'dev_off_door',
    device_label: '办公室门禁',
    created_at: agoIso(1000 * 60 * 45),
  },
  {
    alert_id: 'alt_5',
    space_id: 'spc_gym_1',
    event_type: 'sensor_threshold',
    status: 'unhandled',
    operator_id: '',
    remark: '',
    device_id: '',
    device_label: '',
    created_at: agoIso(1000 * 60 * 60 * 3),
  },
  {
    alert_id: 'alt_6',
    space_id: 'spc_std_a102',
    event_type: 'device_offline',
    status: 'unhandled',
    operator_id: '',
    remark: '',
    device_id: 'dev_cc33dd44',
    device_label: '电子班牌',
    created_at: agoIso(1000 * 60 * 55),
  },
  {
    alert_id: 'alt_7',
    space_id: 'spc_lab_1',
    event_type: 'sensor_threshold',
    status: 'unhandled',
    operator_id: '',
    remark: '',
    device_id: '',
    device_label: '',
    created_at: agoIso(1000 * 60 * 150),
  },
  {
    alert_id: 'alt_8',
    space_id: 'spc_lib_1',
    event_type: 'face_login_failed',
    status: 'handling',
    operator_id: 'u_admin_1',
    remark: '已通知安保到场核实',
    device_id: '',
    device_label: '',
    created_at: agoIso(1000 * 60 * 210),
  },
  {
    alert_id: 'alt_9',
    space_id: 'spc_canteen_1',
    event_type: 'sensor_threshold',
    status: 'unhandled',
    operator_id: '',
    remark: '',
    device_id: '',
    device_label: '',
    created_at: agoIso(1000 * 60 * 300),
  },
  {
    alert_id: 'alt_10',
    space_id: 'spc_b7bee44d',
    event_type: 'device_offline',
    status: 'unhandled',
    operator_id: '',
    remark: '',
    device_id: 'dev_99aabbcc',
    device_label: '后门门禁',
    created_at: agoIso(1000 * 60 * 360),
  },
]

// 演示账号库：与 wechat-gateway/scripts/mock-go-backend.py 的 ACCOUNTS 对齐。
// username -> { password, user_id, role, name }
const ACCOUNTS = {
  admin: { password: 'admin123', user_id: 'u_admin_1', role: 'admin', name: '管理员' },
  li:    { password: '123456',   user_id: 'u_teacher_1', role: 'teacher', name: '李老师' },
  wang:  { password: '123456',   user_id: 'u_teacher_2', role: 'teacher', name: '王老师' },
}

// operator_id -> 姓名（复用 ACCOUNTS；未命中回退到 id 本身）。
function operatorName(id) {
  const acc = Object.values(ACCOUNTS).find((a) => a.user_id === id)
  return acc ? acc.name : id
}

// 运维日志 mock：handleAlert 成功后追加；operationLogs()/alertTimeline() 据此过滤/分页。
// 预置 2 条对齐 mock 自身已处理的告警（alt_3=resolved、alt_8=handling），让详情
// 时间线开箱可见。注意：这是 mock 的 10 条告警数据集，并非 seed_dev.sql 的 5 条。
const operationLogs = [
  {
    id: 1, op_type: 'alert_handle', user_id: 'u_admin_1', operator_name: '管理员',
    space_id: 'spc_a8acdd5c', scene_id: '', success_count: 1, failed_count: 0,
    detail: { alert_id: 'alt_3', event_type: 'face_login_failed', status: 'resolved', remark: '已处理' },
    created_at: agoIso(1000 * 60 * 60),
  },
  {
    id: 2, op_type: 'alert_handle', user_id: 'u_admin_1', operator_name: '管理员',
    space_id: 'spc_lib_1', scene_id: '', success_count: 1, failed_count: 0,
    detail: { alert_id: 'alt_8', event_type: 'face_login_failed', status: 'handling', remark: '已通知安保到场核实' },
    created_at: agoIso(1000 * 60 * 30),
  },
]
let opLogId = 3

// 门禁白名单 mock（7.5③）：{space_id, device_id, label}。mark/unmark 就地增删。
const doorDevices = []

// 空间类型 mock（对齐 format.js SPACE_TYPES + migration_v15.sql 种子）。
const spaceTypes = [
  { code: 'standard', name: '标准教室', sort_order: 0, enabled: true, icon: '' },
  { code: 'lecture', name: '多媒体报告厅', sort_order: 1, enabled: true, icon: '' },
  { code: 'office', name: '办公室', sort_order: 2, enabled: true, icon: '' },
  { code: 'gym', name: '体育馆', sort_order: 3, enabled: true, icon: '' },
  { code: 'lab', name: '实验室', sort_order: 4, enabled: true, icon: '' },
  { code: 'library', name: '图书馆', sort_order: 5, enabled: true, icon: '' },
  { code: 'canteen', name: '食堂', sort_order: 6, enabled: true, icon: '' },
]

// 场景 mock（按教室共享）：{scene_id, space_id, name, kind, device_states(数组)}。
const scenes = []
// 各教室当前激活场景：space_id -> scene_id。
const activeSceneBySpace = {}
// 已懒种默认场景的教室。
const seededSpaces = new Set()

// 空间授权：与网关 main.cpp 的 user_spaces seed 对齐（admin 全量，教师按绑定）。
const USER_SPACES = {
  u_admin_1: ['spc_a8acdd5c', 'spc_std_a102', 'spc_b7bee44d', 'spc_office_1', 'spc_gym_1', 'spc_lab_1', 'spc_lib_1', 'spc_canteen_1'],
  u_teacher_1: ['spc_a8acdd5c', 'spc_std_a102', 'spc_office_1'],
  u_teacher_2: ['spc_b7bee44d'],
}

// 教师名单（从 ACCOUNTS 派生，供绑定管理页展示）。
const TEACHERS = Object.values(ACCOUNTS)
  .filter((a) => a.role === 'teacher')
  .map((a) => ({ user_id: a.user_id, name: a.name }))

// 今日日期（YYYY-MM-DD），供进出记录 mock 对齐默认筛选。
function todayStr() {
  const d = new Date()
  const p = (n) => (n < 10 ? '0' + n : '' + n)
  return `${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())}`
}

// 进出记录 mock：脱敏字段（姓名/时间/教室/结果/方式），与网关 access_records 对齐。
const ACCESS_RECORDS = [
  { name: '李老师', time: `${todayStr()}T08:12:00`, space_id: 'spc_a8acdd5c', result: 'login', auth_type: 'face' },
  { name: '陌生人', time: `${todayStr()}T08:15:00`, space_id: 'spc_a8acdd5c', result: 'denied', auth_type: 'face' },
  { name: '王老师', time: `${todayStr()}T08:20:00`, space_id: 'spc_b7bee44d', result: 'login', auth_type: 'face' },
  { name: '王老师', time: `${todayStr()}T08:25:00`, space_id: 'spc_b7bee44d', result: 'login', auth_type: 'card' },
]

// 登录态直接复用 store（唯一来源），避免 mock 本地再存一份导致与页面登录态漂移。
// login 返回后由 login.vue 统一 applySession 写入 store；logout 时 store 清空即同步登出。
function currentUser() {
  return isLoggedIn() ? { user_id: store.userId, role: store.role, name: store.name } : null
}

const delay = (data, ms = 250) => new Promise((resolve) => setTimeout(() => resolve(data), ms))

export const mockApi = {
  // 密码登录：校验账号 + 密码，返回对应的真实身份（不再是「其它账号都当李老师」）。
  login(username, password) {
    const acct = ACCOUNTS[username]
    if (!acct || acct.password !== password) return Promise.reject(new Error('账号或密码错误'))
    return delay({
      token: 'mock-token-' + acct.user_id,
      user_id: acct.user_id,
      role: acct.role,
      name: acct.name,
    })
  },

  me() {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    return delay({ user_id: u.user_id, role: u.role })
  },

  // 空间：按账号的 user_spaces 授权返回；admin 全量，教师只看自己被授权的教室
  spaces() {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    // admin 看全部启用教室（含管理员新建的）；teacher 按绑定过滤。
    if (u.role === 'admin') {
      return delay(spaces.filter((s) => s.enabled).map((s) => ({ ...s })))
    }
    const allowed = USER_SPACES[u.user_id] || []
    return delay(spaces.filter((s) => s.enabled && allowed.includes(s.space_id)).map((s) => ({ ...s })))
  },

  // 设备：按 space_id 返回；访问未授权空间会被拒
  devices(spaceId) {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    // admin 看全部教室（含管理员新建的、暂无设备的）；teacher 按绑定过滤。
    if (u.role === 'admin') {
      const target = spaceId || Object.keys(devices)[0] || ''
      return delay({ devices: (devices[target] || []).map((d) => toDeviceDoc(d, target)) })
    }
    const allowed = USER_SPACES[u.user_id] || []
    const target = spaceId || allowed[0]
    if (!allowed.includes(target)) return Promise.reject(new Error('无权限访问该空间'))
    return delay({ devices: (devices[target] || []).map((d) => toDeviceDoc(d, target)) })
  },

  // 告警：按账号授权空间过滤 + 服务端筛选/分页（复刻后端 /api/miniapp/alerts 信封）；
  // teacher 脱敏 operator_id / remark。
  alerts(filters) {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    const f = filters || {}
    const allowed = USER_SPACES[u.user_id] || []
    let list = alerts
      .filter((a) => u.role === 'admin' || allowed.includes(a.space_id))
      .map((a) => ({ ...a }))
    if (u.role !== 'admin') {
      list.forEach((a) => {
        a.operator_id = ''
        a.operator_name = ''
        a.remark = ''
      })
    } else {
      list.forEach((a) => {
        a.operator_name = a.operator_id ? operatorName(a.operator_id) : ''
      })
    }
    if (f.space_id) {
      if (u.role !== 'admin' && !allowed.includes(f.space_id)) {
        return Promise.reject(new Error('无权限访问该教室'))
      }
      list = list.filter((a) => a.space_id === f.space_id)
    }
    if (f.event_type) list = list.filter((a) => a.event_type === f.event_type)
    if (f.status) list = list.filter((a) => a.status === f.status)
    if (f.from) list = list.filter((a) => a.created_at >= f.from)
    if (f.to) list = list.filter((a) => a.created_at <= f.to)
    list = list.slice().sort((a, b) =>
      b.created_at < a.created_at ? -1 : b.created_at > a.created_at ? 1 : 0
    )
    const p = f.page || 1
    const ps = f.page_size || 20
    const total = list.length
    const rows = list.slice((p - 1) * ps, (p - 1) * ps + ps)
    return delay({ alerts: rows, total, page: p, page_size: ps, has_more: p * ps < total })
  },

  // 处理告警：仅 admin；status 支持 'handling'（处理中）/ 'resolved'（已处理）
  handleAlert(id, remark, status) {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    if (u.role !== 'admin') return Promise.reject(new Error('仅管理员可处理告警'))
    const a = alerts.find((x) => x.alert_id === id)
    if (!a) return Promise.reject(new Error('告警不存在'))
    const next = status === 'handling' ? 'handling' : 'resolved'
    a.status = next
    a.operator_id = u.user_id
    a.operator_name = u.name || operatorName(u.user_id)
    a.remark = remark || (next === 'handling' ? '处理中' : '已处理')
    // 运维日志：处理告警 → 追加一条 alert_handle 日志（读接口 operationLogs 可见）。
    operationLogs.push({
      id: opLogId++,
      op_type: 'alert_handle',
      user_id: u.user_id,
      operator_name: u.name || operatorName(u.user_id),
      space_id: a.space_id,
      scene_id: '',
      success_count: 1,
      failed_count: 0,
      detail: { alert_id: id, event_type: a.event_type, status: next, remark: a.remark },
      created_at: new Date().toISOString(),
    })
    return delay({ ok: true, status: next })
  },

  // 设备控制：复刻门禁白名单二次确认（428）
  control(payload) {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    const isDoor = doorDevices.some(
      (d) => d.space_id === payload.space_id && d.device_id === payload.device_id
    )
    if (isDoor && payload.command === 'toggle') {
      return Promise.reject(Object.assign(new Error('门禁不支持切换，请用开/关'), { status: 400 }))
    }
    if (isDoor && payload.command === 'off' && !payload.confirm) {
      return Promise.reject(Object.assign(new Error('开门需二次确认'), { code: 'CONFIRM_REQUIRED', status: 428 }))
    }
    // 手动控制 → 更新设备开关态 + 清空该空间的激活场景。
    if (payload && payload.device_id) switchStates[payload.device_id] = payload.command
    if (payload && payload.space_id) delete activeSceneBySpace[payload.space_id]
    return delay({ ok: true, ...payload, echo: 'mock: 指令已下发' })
  },

  // 门禁白名单（读：admin/teacher；写：仅 admin）
  doorDevices(spaceId) {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    const list = spaceId ? doorDevices.filter((d) => d.space_id === spaceId) : doorDevices.slice()
    return delay({ doors: list.map((d) => ({ ...d })) })
  },
  markDoor(payload) {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    if (u.role !== 'admin') return Promise.reject(new Error('仅管理员可标记门禁'))
    const idx = doorDevices.findIndex(
      (d) => d.space_id === payload.space_id && d.device_id === payload.device_id
    )
    const entry = { space_id: payload.space_id, device_id: payload.device_id, label: payload.label || '' }
    if (idx >= 0) doorDevices[idx] = entry
    else doorDevices.push(entry)
    return delay({ ok: true })
  },
  unmarkDoor(spaceId, deviceId) {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    if (u.role !== 'admin') return Promise.reject(new Error('仅管理员可取消门禁标记'))
    const idx = doorDevices.findIndex((d) => d.space_id === spaceId && d.device_id === deviceId)
    if (idx >= 0) doorDevices.splice(idx, 1)
    return delay({ ok: true })
  },

  // 场景：读登录可读（懒种默认开启/离开）；写 admin/teacher 按空间权限。
  scenes(spaceId) {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    if (u.role !== 'admin') {
      const allowed = USER_SPACES[u.user_id] || []
      if (!allowed.includes(spaceId)) return Promise.reject(new Error('无权限访问该空间'))
    }
    if (!seededSpaces.has(spaceId)) {
      seededSpaces.add(spaceId)
      scenes.push({ scene_id: 'scn_all_on_' + spaceId, space_id: spaceId, name: '开启模式', kind: 'all_on', device_states: [] })
      scenes.push({ scene_id: 'scn_all_off_' + spaceId, space_id: spaceId, name: '离开模式', kind: 'all_off', device_states: [] })
    }
    const list = scenes.filter((s) => s.space_id === spaceId).map((s) => ({ ...s, device_states: s.device_states.slice() }))
    return delay({ scenes: list, active_scene_id: activeSceneBySpace[spaceId] || '' })
  },

  createScene(payload) {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    const spaceId = (payload && payload.space_id) || ''
    if (u.role !== 'admin') {
      const allowed = USER_SPACES[u.user_id] || []
      if (!allowed.includes(spaceId)) return Promise.reject(new Error('无权限访问该空间'))
    }
    const name = (payload && payload.name) || ''
    const ds = (payload && payload.device_states) || []
    if (!name) return Promise.reject(new Error('场景名必填'))
    if (!Array.isArray(ds) || !ds.length) return Promise.reject(new Error('至少选一台设备'))
    const scene = { scene_id: 'scn_' + Date.now(), space_id: spaceId, name, kind: 'custom', device_states: ds.slice() }
    scenes.push(scene)
    return delay({ ok: true, scene_id: scene.scene_id })
  },

  updateScene(sceneId, payload) {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    const s = scenes.find((x) => x.scene_id === sceneId)
    if (!s) return Promise.reject(new Error('场景不存在'))
    if (u.role !== 'admin') {
      const allowed = USER_SPACES[u.user_id] || []
      if (!allowed.includes(s.space_id)) return Promise.reject(new Error('无权限访问该空间'))
    }
    if (payload && payload.name != null) s.name = payload.name
    if (payload && payload.device_states != null) s.device_states = payload.device_states.slice()
    return delay({ ok: true })
  },

  deleteScene(sceneId) {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    const s = scenes.find((x) => x.scene_id === sceneId)
    if (!s) return Promise.reject(new Error('场景不存在'))
    if (u.role !== 'admin') {
      const allowed = USER_SPACES[u.user_id] || []
      if (!allowed.includes(s.space_id)) return Promise.reject(new Error('无权限访问该空间'))
    }
    const idx = scenes.findIndex((x) => x.scene_id === sceneId)
    if (idx >= 0) scenes.splice(idx, 1)
    if (activeSceneBySpace[s.space_id] === sceneId) delete activeSceneBySpace[s.space_id]
    return delay({ ok: true })
  },

  executeScene(sceneId) {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    const s = scenes.find((x) => x.scene_id === sceneId)
    if (!s) return Promise.reject(new Error('场景不存在'))
    if (u.role !== 'admin') {
      const allowed = USER_SPACES[u.user_id] || []
      if (!allowed.includes(s.space_id)) return Promise.reject(new Error('无权限访问该空间'))
    }
    // 更新设备开关态：custom 按 device_states；all_on/all_off 覆盖全部可控设备（除传感器/门禁）。
    let applied = 0
    if (s.kind === 'custom') {
      s.device_states.forEach((x) => { switchStates[x.device_id] = x.command })
      applied = s.device_states.length
    } else {
      const cmd = s.kind === 'all_on' ? 'on' : 'off'
      ;(devices[s.space_id] || []).forEach((dev) => {
        if (dev.type !== 'sensor' && dev.type !== 'door') { switchStates[dev.device_id] = cmd; applied++ }
      })
    }
    activeSceneBySpace[s.space_id] = sceneId
    return delay({ ok: true, scene_id: sceneId, space_id: s.space_id, applied, warning: '' })
  },

  // 订阅授权
  subscribe(payload) {
    if (!currentUser()) return Promise.reject(new Error('未登录'))
    return delay({ ok: true, ...payload })
  },

  getSubscriptions() {
    if (!currentUser()) return Promise.reject(new Error('未登录'))
    return delay({ ok: true, templates: {} })
  },

  // 订阅消息模板 ID（与网关 config 的 3 个唯一模板对齐：
  // device_offline / peripheral_offline 共用 tmpl_001）
  notifyTemplates() {
    if (!currentUser()) return Promise.reject(new Error('未登录'))
    return delay({ template_ids: ['tmpl_001', 'tmpl_002', 'tmpl_003'] })
  },

  // 教师-教室绑定：仅 admin。就地改 USER_SPACES，使 spaces() 即时反映（演示即时生效）。
  spaceBindings() {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    if (u.role !== 'admin') return Promise.reject(new Error('仅管理员可管理绑定'))
    const teachers = TEACHERS.map((t) => ({
      user_id: t.user_id,
      name: t.name,
      space_ids: (USER_SPACES[t.user_id] || []).slice(),
    }))
    return delay({ teachers })
  },

  bindSpace(userId, spaceId) {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    if (u.role !== 'admin') return Promise.reject(new Error('仅管理员可管理绑定'))
    if (!spaces.some((s) => s.space_id === spaceId)) return Promise.reject(new Error('教室不存在'))
    const list = USER_SPACES[userId] || (USER_SPACES[userId] = [])
    if (!list.includes(spaceId)) list.push(spaceId)
    return delay({ ok: true })
  },

  unbindSpace(userId, spaceId) {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    if (u.role !== 'admin') return Promise.reject(new Error('仅管理员可管理绑定'))
    USER_SPACES[userId] = (USER_SPACES[userId] || []).filter((s) => s !== spaceId)
    return delay({ ok: true })
  },

  // 空间类型：读登录可读（返回全部含停用，按 sort_order）；写仅 admin。
  spaceTypes() {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    const list = spaceTypes.slice().sort((a, b) => a.sort_order - b.sort_order)
    return delay(list.map((t) => ({ ...t })))
  },

  createSpaceType(payload) {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    if (u.role !== 'admin') return Promise.reject(new Error('仅管理员可管理空间类型'))
    const code = (payload && payload.code) || ''
    const name = (payload && payload.name) || ''
    if (!code || !name) return Promise.reject(new Error('code 和 name 必填'))
    if (spaceTypes.some((t) => t.code === code)) return Promise.reject(new Error('类型 code 已存在'))
    const sortOrder =
      payload && payload.sort_order != null
        ? payload.sort_order
        : spaceTypes.reduce((m, t) => Math.max(m, t.sort_order), -1) + 1
    spaceTypes.push({ code, name, sort_order: sortOrder, enabled: true, icon: (payload && payload.icon) || '' })
    return delay({ ok: true })
  },

  updateSpaceType(code, payload) {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    if (u.role !== 'admin') return Promise.reject(new Error('仅管理员可管理空间类型'))
    const t = spaceTypes.find((x) => x.code === code)
    if (!t) return Promise.reject(new Error('空间类型不存在'))
    if (payload && payload.name != null) t.name = payload.name
    if (payload && payload.icon != null) t.icon = payload.icon
    if (payload && payload.enabled != null) t.enabled = payload.enabled
    return delay({ ok: true })
  },

  disableSpaceType(code) {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    if (u.role !== 'admin') return Promise.reject(new Error('仅管理员可管理空间类型'))
    const t = spaceTypes.find((x) => x.code === code)
    if (!t) return Promise.reject(new Error('空间类型不存在'))
    t.enabled = false
    return delay({ ok: true })
  },

  reorderSpaceTypes(codes) {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    if (u.role !== 'admin') return Promise.reject(new Error('仅管理员可管理空间类型'))
    codes.forEach((code, i) => {
      const t = spaceTypes.find((x) => x.code === code)
      if (t) t.sort_order = i
    })
    return delay({ ok: true })
  },

  // 教室管理：读 admin 列表；写仅 admin。
  adminSpaces() {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    if (u.role !== 'admin') return Promise.reject(new Error('仅管理员可管理教室'))
    return delay(spaces.slice().map((s) => ({ ...s })))
  },

  createSpace(payload) {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    if (u.role !== 'admin') return Promise.reject(new Error('仅管理员可管理教室'))
    const name = (payload && payload.name) || ''
    const type = (payload && payload.type) || ''
    if (!name || !type) return Promise.reject(new Error('教室名和类型必填'))
    if (!spaceTypes.some((t) => t.code === type)) return Promise.reject(new Error('类型不存在'))
    const spaceId = (payload && payload.space_id) || ('spc_' + Date.now())
    if (spaces.some((s) => s.space_id === spaceId)) return Promise.reject(new Error('教室已存在'))
    spaces.push({ space_id: spaceId, name, type, enabled: true, source: 'admin' })
    return delay({ ok: true, space_id: spaceId })
  },

  updateSpace(spaceId, payload) {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    if (u.role !== 'admin') return Promise.reject(new Error('仅管理员可管理教室'))
    const s = spaces.find((x) => x.space_id === spaceId)
    if (!s) return Promise.reject(new Error('教室不存在'))
    if (payload && payload.name != null) s.name = payload.name
    if (payload && payload.type != null) {
      if (!spaceTypes.some((t) => t.code === payload.type)) return Promise.reject(new Error('类型不存在'))
      s.type = payload.type
    }
    if (payload && payload.enabled != null) s.enabled = payload.enabled
    s.source = 'admin'  // 管理员改动即接管
    return delay({ ok: true })
  },

  disableSpace(spaceId) {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    if (u.role !== 'admin') return Promise.reject(new Error('仅管理员可管理教室'))
    const s = spaces.find((x) => x.space_id === spaceId)
    if (!s) return Promise.reject(new Error('教室不存在'))
    s.enabled = false
    return delay({ ok: true })
  },

  // 进出记录：按权限 + 空间/日期/方式/分页过滤，脱敏返回。
  accessRecords(spaceId, date, page, pageSize, authType) {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    const allowed = USER_SPACES[u.user_id] || []
    let list = ACCESS_RECORDS.filter(
      (r) => u.role === 'admin' || allowed.includes(r.space_id)
    )
    if (spaceId) {
      if (u.role !== 'admin' && !allowed.includes(spaceId)) {
        return Promise.reject(new Error('无权限访问该教室'))
      }
      list = list.filter((r) => r.space_id === spaceId)
    }
    if (date) list = list.filter((r) => r.time.slice(0, 10) === date)
    if (authType) list = list.filter((r) => r.auth_type === authType)
    list = list.slice().sort((a, b) => (b.time < a.time ? -1 : b.time > a.time ? 1 : 0))
    const p = page || 1
    const ps = pageSize || 50
    const total = list.length
    const records = list.slice((p - 1) * ps, (p - 1) * ps + ps)
    return delay({ records, total, page: p, page_size: ps, has_more: p * ps < total })
  },

  // 运维日志：仅 admin；按 op_type/space_id/user_id/from/to 过滤 + 分页。
  // 结构对齐后端 GET /api/miniapp/operation-logs。
  operationLogs(filters) {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    if (u.role !== 'admin') return Promise.reject(new Error('仅管理员可查看运维日志'))
    const f = filters || {}
    let list = operationLogs.slice()
    if (f.op_type) list = list.filter((l) => l.op_type === f.op_type)
    if (f.space_id) list = list.filter((l) => l.space_id === f.space_id)
    if (f.user_id) list = list.filter((l) => l.user_id === f.user_id)
    if (f.from) list = list.filter((l) => l.created_at >= f.from)
    if (f.to) list = list.filter((l) => l.created_at <= f.to)
    list = list.slice().sort((a, b) => b.id - a.id)
    const p = f.page || 1
    const ps = f.page_size || 20
    const total = list.length
    const logs = list.slice((p - 1) * ps, (p - 1) * ps + ps)
    return delay({ logs, total, page: p, page_size: ps, has_more: p * ps < total })
  },

  // 告警详情时间线：仅 admin；返回 {alert, timeline}（timeline 按 id ASC = 时间序旧→新）。
  alertTimeline(id) {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    if (u.role !== 'admin') return Promise.reject(new Error('仅管理员可查看处理过程'))
    const a = alerts.find((x) => x.alert_id === id)
    if (!a) return Promise.reject(new Error('告警不存在'))
    const timeline = operationLogs
      .filter((l) => l.detail && l.detail.alert_id === id)
      .slice()
      .sort((x, y) => x.id - y.id)
      .map((l) => ({
        id: l.id,
        status: l.detail.status,
        user_id: l.user_id,
        operator_name: l.operator_name || operatorName(l.user_id),
        remark: l.detail.remark,
        created_at: l.created_at,
      }))
    return delay({
      alert: {
        alert_id: a.alert_id,
        space_id: a.space_id,
        event_type: a.event_type,
        status: a.status,
        operator_id: a.operator_id,
        operator_name: a.operator_name || (a.operator_id ? operatorName(a.operator_id) : ''),
        remark: a.remark,
        device_id: a.device_id || '',
        device_label: a.device_label || '',
        created_at: a.created_at,
        updated_at: a.updated_at || a.created_at,
      },
      timeline,
    })
  },

  // 单条告警详情：admin 全量 / teacher 本空间脱敏（operator 三字段空串）；
  // teacher 越权与不存在一致 reject 且带 status=404（对齐后端，前端据此走空态而非弹错）。
  alertDetail(id) {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    const notFound = () => {
      const e = new Error('告警不存在')
      e.status = 404
      return Promise.reject(e)
    }
    const a = alerts.find((x) => x.alert_id === id)
    if (!a) return notFound()
    const allowed = USER_SPACES[u.user_id] || []
    if (u.role === 'teacher' && !allowed.includes(a.space_id)) {
      return notFound()
    }
    const isAdmin = u.role === 'admin'
    return delay({
      alert: {
        alert_id: a.alert_id,
        space_id: a.space_id,
        event_type: a.event_type,
        status: a.status,
        operator_id: isAdmin ? (a.operator_id || '') : '',
        operator_name: isAdmin ? (a.operator_name || (a.operator_id ? operatorName(a.operator_id) : '')) : '',
        remark: isAdmin ? (a.remark || '') : '',
        device_id: a.device_id || '',
        device_label: a.device_label || '',
        created_at: a.created_at,
        updated_at: a.updated_at || a.created_at,
      },
    })
  },

  // 告警统计：按权限过滤（admin 全量 / teacher 绑定教室）+ 全量聚合，
  // 不套列表的分页/筛选参数。by_status 三态补 0，by_space 仅 admin。
  alertStats() {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    const allowed = USER_SPACES[u.user_id] || []
    const list = alerts.filter((a) => u.role === 'admin' || allowed.includes(a.space_id))
    const byStatus = { unhandled: 0, handling: 0, resolved: 0 }
    const byEventType = {}
    const bySpace = {}
    const bySpaceUnhandled = {}
    list.forEach((a) => {
      if (byStatus[a.status] !== undefined) byStatus[a.status]++
      byEventType[a.event_type] = (byEventType[a.event_type] || 0) + 1
      bySpace[a.space_id] = (bySpace[a.space_id] || 0) + 1
      if (a.status === 'unhandled') {
        bySpaceUnhandled[a.space_id] = (bySpaceUnhandled[a.space_id] || 0) + 1
      }
    })
    const body = { by_status: byStatus, by_event_type: byEventType }
    if (u.role === 'admin') {
      body.by_space = Object.keys(bySpace).map((sid) => ({ space_id: sid, count: bySpace[sid] }))
      body.by_space_unhandled = Object.keys(bySpaceUnhandled).map((sid) => ({ space_id: sid, count: bySpaceUnhandled[sid] }))
    }
    return delay(body)
  },
}
