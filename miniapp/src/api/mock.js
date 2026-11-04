// 本地 mock：镜像 wechat-gateway 的 dev seed 数据与 mock-go-backend 的返回结构，
// 并复刻后端的关键语义（空间越权、教师脱敏、高危指令确认、仅管理员可处理告警）。
// 用途：不启动后端、不接微信授权，就能在浏览器/开发者工具里预览整套界面。
import { store, isLoggedIn } from '../store/index'

// 空间：带类型（standard 标准教室 / lecture 多媒体报告厅 / office 办公室）。
// 第一级按类型区分，第二级才是类型下的各个教室。
const spaces = [
  { space_id: 'spc_a8acdd5c', name: 'A101 教室', type: 'standard', enabled: true, source: 'edge' },
  { space_id: 'spc_std_a102', name: 'A102 教室', type: 'standard', enabled: true, source: 'edge' },
  { space_id: 'spc_std_a103', name: 'A103 教室', type: 'standard', enabled: true, source: 'edge' },
  { space_id: 'spc_std_a104', name: 'A104 教室', type: 'standard', enabled: true, source: 'edge' },
  { space_id: 'spc_std_a201', name: 'A201 教室', type: 'standard', enabled: true, source: 'edge' },
  { space_id: 'spc_std_a202', name: 'A202 教室', type: 'standard', enabled: true, source: 'edge' },
  { space_id: 'spc_std_b301', name: 'B301 教室', type: 'standard', enabled: true, source: 'edge' },
  { space_id: 'spc_b7bee44d', name: '多媒体报告厅', type: 'lecture', enabled: true, source: 'edge' },
  { space_id: 'spc_lec_2', name: '学术报告厅', type: 'lecture', enabled: true, source: 'edge' },
  { space_id: 'spc_lec_3', name: '阶梯教室 301', type: 'lecture', enabled: true, source: 'edge' },
  { space_id: 'spc_office_1', name: '教师办公室', type: 'office', enabled: true, source: 'edge' },
  { space_id: 'spc_office_2', name: '教务办公室', type: 'office', enabled: true, source: 'edge' },
  { space_id: 'spc_gym_1', name: '体育馆', type: 'gym', enabled: true, source: 'edge' },
  { space_id: 'spc_lab_1', name: '化学实验室', type: 'lab', enabled: true, source: 'edge' },
  { space_id: 'spc_lab_2', name: '物理实验室', type: 'lab', enabled: true, source: 'edge' },
  { space_id: 'spc_lab_3', name: '生物实验室', type: 'lab', enabled: true, source: 'edge' },
  { space_id: 'spc_lib_1', name: '图书馆', type: 'library', enabled: true, source: 'edge' },
  { space_id: 'spc_lib_2', name: '电子阅览室', type: 'library', enabled: true, source: 'edge' },
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
  spc_std_a103: [
    { device_id: 'dev_a103_light', label: '教室照明', type: 'light', online: true },
    { device_id: 'dev_a103_sign', label: '电子班牌', type: 'sign', online: true },
    { device_id: 'dev_a103_screen', label: '综合屏', type: 'screen', online: false },
    { device_id: 'dev_a103_door', label: '前门门禁', type: 'door', online: true },
    { device_id: 'dev_a103_sens', label: '环境传感器', type: 'sensor', online: true },
  ],
  spc_std_a104: [
    { device_id: 'dev_a104_light', label: '教室照明', type: 'light', online: true },
    { device_id: 'dev_a104_sign', label: '电子班牌', type: 'sign', online: true },
    { device_id: 'dev_a104_screen', label: '综合屏', type: 'screen', online: true },
    { device_id: 'dev_a104_door', label: '前门门禁', type: 'door', online: true },
    { device_id: 'dev_a104_sens', label: '环境传感器', type: 'sensor', online: true },
  ],
  spc_std_a201: [
    { device_id: 'dev_a201_light', label: '教室照明', type: 'light', online: true },
    { device_id: 'dev_a201_sign', label: '电子班牌', type: 'sign', online: false },
    { device_id: 'dev_a201_screen', label: '综合屏', type: 'screen', online: true },
    { device_id: 'dev_a201_door', label: '前门门禁', type: 'door', online: true },
    { device_id: 'dev_a201_sens', label: '环境传感器', type: 'sensor', online: true },
  ],
  spc_std_a202: [
    { device_id: 'dev_a202_light', label: '教室照明', type: 'light', online: true },
    { device_id: 'dev_a202_sign', label: '电子班牌', type: 'sign', online: true },
    { device_id: 'dev_a202_screen', label: '综合屏', type: 'screen', online: true },
    { device_id: 'dev_a202_door', label: '前门门禁', type: 'door', online: true },
    { device_id: 'dev_a202_sens', label: '环境传感器', type: 'sensor', online: true },
  ],
  spc_std_b301: [
    { device_id: 'dev_b301_light', label: '教室照明', type: 'light', online: true },
    { device_id: 'dev_b301_sign', label: '电子班牌', type: 'sign', online: true },
    { device_id: 'dev_b301_screen', label: '综合屏', type: 'screen', online: true },
    { device_id: 'dev_b301_door', label: '后门门禁', type: 'door', online: false },
  ],
  spc_lec_2: [
    { device_id: 'dev_lec2_main', label: '学术厅主屏', type: 'screen', online: true },
    { device_id: 'dev_lec2_side', label: '学术厅副屏', type: 'screen', online: true },
    { device_id: 'dev_lec2_light', label: '报告厅照明', type: 'light', online: true },
    { device_id: 'dev_lec2_door', label: '入口门禁', type: 'door', online: true },
    { device_id: 'dev_lec2_sens', label: '环境传感器', type: 'sensor', online: true },
  ],
  spc_lec_3: [
    { device_id: 'dev_lec3_main', label: '阶梯教室主屏', type: 'screen', online: false },
    { device_id: 'dev_lec3_light', label: '阶梯教室照明', type: 'light', online: true },
    { device_id: 'dev_lec3_door', label: '入口门禁', type: 'door', online: true },
    { device_id: 'dev_lec3_sens', label: '环境传感器', type: 'sensor', online: true },
  ],
  spc_office_2: [
    { device_id: 'dev_of2_light', label: '办公区照明', type: 'light', online: true },
    { device_id: 'dev_of2_door', label: '办公室门禁', type: 'door', online: true },
    { device_id: 'dev_of2_sens', label: '温湿度传感器', type: 'sensor', online: true },
  ],
  spc_lab_2: [
    { device_id: 'dev_lab2_light', label: '实验室照明', type: 'light', online: true },
    { device_id: 'dev_lab2_door', label: '实验室门禁', type: 'door', online: true },
    { device_id: 'dev_lab2_sens', label: '环境传感器', type: 'sensor', online: false },
  ],
  spc_lab_3: [
    { device_id: 'dev_lab3_light', label: '实验室照明', type: 'light', online: true },
    { device_id: 'dev_lab3_door', label: '实验室门禁', type: 'door', online: true },
    { device_id: 'dev_lab3_sens', label: '环境传感器', type: 'sensor', online: true },
  ],
  spc_lib_2: [
    { device_id: 'dev_lib2_light', label: '阅览室照明', type: 'light', online: true },
    { device_id: 'dev_lib2_screen', label: '查询终端大屏', type: 'screen', online: true },
    { device_id: 'dev_lib2_door', label: '阅览室门禁', type: 'door', online: true },
    { device_id: 'dev_lib2_sens', label: '环境传感器', type: 'sensor', online: true },
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
    d.app = JSON.stringify({ temperature: 25.1, humidity: 60, co2: 800, pm25: 18 })
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

// 反查设备原始类型（toDeviceDoc 会把类型改写成网关字段，这里还原成 mock 的类型键）。
function devRawType(doc) {
  if (doc.device_type === 'fuhe-board') return 'sign'
  if (doc.device_type === 'fuhe-screen') return 'screen'
  if (doc.zb_role) return doc.zb_role
  if (doc.zb_type === 'sensor') return 'sensor'
  return ''
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
  {
    alert_id: 'alt_11',
    space_id: 'spc_std_a103',
    event_type: 'device_offline',
    status: 'unhandled',
    operator_id: '',
    remark: '',
    device_id: 'dev_a103_screen',
    device_label: '综合屏',
    created_at: agoIso(1000 * 60 * 12),
  },
  {
    alert_id: 'alt_12',
    space_id: 'spc_std_a201',
    event_type: 'device_offline',
    status: 'handling',
    operator_id: 'u_admin_1',
    remark: '已联系厂商更换班牌网关',
    device_id: 'dev_a201_sign',
    device_label: '电子班牌',
    created_at: agoIso(1000 * 60 * 50),
  },
  {
    alert_id: 'alt_13',
    space_id: 'spc_lec_3',
    event_type: 'device_offline',
    status: 'unhandled',
    operator_id: '',
    remark: '',
    device_id: 'dev_lec3_main',
    device_label: '阶梯教室主屏',
    created_at: agoIso(1000 * 60 * 95),
  },
  {
    alert_id: 'alt_14',
    space_id: 'spc_std_a104',
    event_type: 'sensor_threshold',
    status: 'unhandled',
    operator_id: '',
    remark: '',
    device_id: '',
    device_label: '',
    created_at: agoIso(1000 * 60 * 20),
  },
  {
    alert_id: 'alt_15',
    space_id: 'spc_lab_2',
    event_type: 'device_offline',
    status: 'unhandled',
    operator_id: '',
    remark: '',
    device_id: 'dev_lab2_sens',
    device_label: '环境传感器',
    created_at: agoIso(1000 * 60 * 240),
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

// 通知目标绑定 mock：{channel, user_id, external_id, external_extra, updated_at}。
// channel 枚举对齐 NotifyBindingService.cpp 的 kKnownChannels（不含 wechat_oa）。
const KNOWN_CHANNELS = ['wechat_miniapp', 'sms', 'dingtalk', 'wecom']
const notifyBindings = []

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
// 注意：u_teacher_2（王老师）在网关 seed 里只绑 spc_b7bee44d，这里多绑
// A101/A102 两个标准教室，仅为演示「我的教室」页的切换教室功能；
// 真实模式下教室列表由网关按绑定返回，以此 mock 为准的演示不受影响。
const USER_SPACES = {
  // 管理员默认拥有全部空间（新增教室自动包含，不用逐个补 id）
  u_admin_1: spaces.map((s) => s.space_id),
  // 教师按绑定：李老师带 3 个标准教室 + 办公室，王老师主要用报告厅
  u_teacher_1: ['spc_a8acdd5c', 'spc_std_a102', 'spc_std_a103', 'spc_std_a104', 'spc_office_1'],
  u_teacher_2: ['spc_b7bee44d', 'spc_lec_2', 'spc_a8acdd5c', 'spc_std_a103'],
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
// 每间教室预置多条，便于演示「今日进出」卡片的收起/查看全部（默认收起显示 2 行）。
const ACCESS_RECORDS = [
  // 多媒体报告厅
  { name: '王老师', time: `${todayStr()}T08:20:00`, space_id: 'spc_b7bee44d', result: 'login', auth_type: 'face' },
  { name: '王老师', time: `${todayStr()}T08:25:00`, space_id: 'spc_b7bee44d', result: 'login', auth_type: 'card' },
  { name: '张老师', time: `${todayStr()}T09:05:00`, space_id: 'spc_b7bee44d', result: 'login', auth_type: 'face' },
  { name: '李老师', time: `${todayStr()}T09:30:00`, space_id: 'spc_b7bee44d', result: 'login', auth_type: 'face' },
  { name: '陈教授', time: `${todayStr()}T10:00:00`, space_id: 'spc_b7bee44d', result: 'login', auth_type: 'face' },
  { name: '学生甲', time: `${todayStr()}T10:15:00`, space_id: 'spc_b7bee44d', result: 'login', auth_type: 'card' },
  { name: '陌生人', time: `${todayStr()}T11:40:00`, space_id: 'spc_b7bee44d', result: 'denied', auth_type: 'face' },
  { name: '保洁王姨', time: `${todayStr()}T12:30:00`, space_id: 'spc_b7bee44d', result: 'login', auth_type: 'card' },
  // A101 标准教室
  { name: '李老师', time: `${todayStr()}T08:12:00`, space_id: 'spc_a8acdd5c', result: 'login', auth_type: 'face' },
  { name: '陌生人', time: `${todayStr()}T08:15:00`, space_id: 'spc_a8acdd5c', result: 'denied', auth_type: 'face' },
  { name: '张老师', time: `${todayStr()}T09:10:00`, space_id: 'spc_a8acdd5c', result: 'login', auth_type: 'face' },
  { name: '王同学', time: `${todayStr()}T10:30:00`, space_id: 'spc_a8acdd5c', result: 'login', auth_type: 'card' },
  { name: '刘老师', time: `${todayStr()}T13:20:00`, space_id: 'spc_a8acdd5c', result: 'login', auth_type: 'face' },
  // A102 标准教室
  { name: '赵老师', time: `${todayStr()}T08:40:00`, space_id: 'spc_std_a102', result: 'login', auth_type: 'face' },
  { name: '孙同学', time: `${todayStr()}T09:50:00`, space_id: 'spc_std_a102', result: 'login', auth_type: 'card' },
  // A103 / A104 / B301 标准教室
  { name: '李老师', time: `${todayStr()}T08:05:00`, space_id: 'spc_std_a103', result: 'login', auth_type: 'face' },
  { name: '陈同学', time: `${todayStr()}T09:20:00`, space_id: 'spc_std_a103', result: 'login', auth_type: 'card' },
  { name: '王老师', time: `${todayStr()}T08:30:00`, space_id: 'spc_std_a104', result: 'login', auth_type: 'face' },
  { name: '周同学', time: `${todayStr()}T10:40:00`, space_id: 'spc_std_a104', result: 'login', auth_type: 'face' },
  { name: '郑老师', time: `${todayStr()}T13:45:00`, space_id: 'spc_std_b301', result: 'login', auth_type: 'card' },
  // 学术报告厅 / 阶梯教室
  { name: '张老师', time: `${todayStr()}T14:10:00`, space_id: 'spc_lec_2', result: 'login', auth_type: 'face' },
  { name: '陌生人', time: `${todayStr()}T14:25:00`, space_id: 'spc_lec_2', result: 'denied', auth_type: 'face' },
  { name: '刘老师', time: `${todayStr()}T15:00:00`, space_id: 'spc_lec_3', result: 'login', auth_type: 'card' },
  // 实验室 / 阅览室
  { name: '孙老师', time: `${todayStr()}T09:35:00`, space_id: 'spc_lab_2', result: 'login', auth_type: 'face' },
  { name: '吴同学', time: `${todayStr()}T11:05:00`, space_id: 'spc_lib_2', result: 'login', auth_type: 'card' },
  // 办公室
  { name: '李老师', time: `${todayStr()}T14:00:00`, space_id: 'spc_office_1', result: 'login', auth_type: 'face' },
]

// 登录态直接复用 store（唯一来源），避免 mock 本地再存一份导致与页面登录态漂移。
// login 返回后由 login.vue 统一 applySession 写入 store；logout 时 store 清空即同步登出。
function currentUser() {
  return isLoggedIn() ? { user_id: store.userId, role: store.role, name: store.name } : null
}

const delay = (data, ms = 250) => new Promise((resolve) => setTimeout(() => resolve(data), ms))

// ---------------------------------------------------------------------------
// AI 助手 mock
// 真实网关的 AI 走 LLM + 工具调用（AiService / AiTools）；本地 mock 用意图匹配
// + 现有 mock 数据生成回复，让不启后端也能完整演示 AI 助手链路
// （对话 / 新建会话 / 历史回填 / 删除会话），返回结构与网关完全一致。
// ---------------------------------------------------------------------------
const aiConversations = []
let aiConvSeq = 0

const AI_DEVICE_TYPE = {
  light: '照明',
  sign: '电子班牌',
  screen: '综合屏',
  door: '门禁',
  sensor: '传感器',
}

// 设备口语别名：用户说「灯」要能命中「教室照明」。
// 只有类型名映射的话，必须一字不差念「照明」，说「灯」就匹配不到。
const AI_DEVICE_ALIAS = {
  light: ['灯', '灯光', '电灯', '照明灯', '灯管', '亮灯', '开灯', '关灯'],
  sign: ['班牌', '门牌', '班牌机', '牌子'],
  screen: ['屏', '大屏', '屏幕', '显示器', '一体机'],
  door: ['门', '门锁', '门禁', '开门', '关门'],
  sensor: ['传感器', '温湿度', '二氧化碳', '环境数据', '探头'],
}

// 设备目标匹配，三级回退：全名 → 类型名 → 口语别名。
function aiMatchDevice(devList, t) {
  const byLabel = devList.find((d) => t.includes(d.label))
  if (byLabel) return byLabel
  const byType = devList.find((d) => {
    const name = AI_DEVICE_TYPE[devRawType(d)]
    return name && t.includes(name)
  })
  if (byType) return byType
  return devList.find((d) => {
    const keys = AI_DEVICE_ALIAS[devRawType(d)] || []
    return keys.some((k) => t.includes(k))
  })
}

const AI_EVENT_TYPE = {
  device_offline: '设备离线',
  peripheral_offline: '外设离线',
  sensor_threshold: '传感器阈值告警',
  face_login_failed: '人脸识别失败',
}

const AI_ALERT_STATUS = {
  unhandled: '未处理',
  handling: '处理中',
  resolved: '已处理',
}

const AI_AUTH_TYPE = { face: '人脸', card: '刷卡' }

// 当前登录用户可访问的空间（admin 全量，teacher 按绑定）。
function aiAccessibleSpaces() {
  const u = currentUser()
  if (!u) return []
  if (u.role === 'admin') return spaces.filter((s) => s.enabled).map((s) => ({ ...s }))
  const allowed = USER_SPACES[u.user_id] || []
  return allowed
    .map((id) => spaces.find((s) => s.space_id === id && s.enabled))
    .filter(Boolean)
    .map((s) => ({ ...s }))
}

// 空间名匹配：编号（A101/A102）优先，其次名称关键词（报告厅/办公室/实验室…）。
// pool 传 spaces 表示在全部空间里找（用于识别「无权限访问」的越权提问）。
function aiFindSpace(text, pool) {
  const t = String(text || '').toUpperCase()
  const list = pool || spaces
  for (const s of list) {
    const m = s.name.match(/[A-Z]+\d+/)
    if (m && t.includes(m[0].toUpperCase())) return s
  }
  for (const s of list) {
    const base = s.name.replace(/教室|房间/g, '')
    if (!base) continue
    for (let i = 0; i < base.length - 1; i++) {
      const kw = base.slice(i)
      if (/^\d+$/.test(kw)) continue // 纯数字关键词太容易误命中
      if (t.includes(kw.toUpperCase())) return s
    }
  }
  return null
}

function aiSpaceTypeName(code) {
  const t = spaceTypes.find((x) => x.code === code)
  return t ? t.name : ''
}

function aiAgo(iso) {
  const diff = Date.now() - new Date(iso).getTime()
  const m = Math.round(diff / 60000)
  if (m < 1) return '刚刚'
  if (m < 60) return m + ' 分钟前'
  const h = Math.round(m / 60)
  if (h < 24) return h + ' 小时前'
  return Math.round(h / 24) + ' 天前'
}

// 懒种默认场景（与 mockApi.scenes 同逻辑）。
function aiEnsureScenes(spaceId) {
  if (seededSpaces.has(spaceId)) return
  seededSpaces.add(spaceId)
  scenes.push({ scene_id: 'scn_all_on_' + spaceId, space_id: spaceId, name: '开启模式', kind: 'all_on', device_states: [] })
  scenes.push({ scene_id: 'scn_all_off_' + spaceId, space_id: spaceId, name: '离开模式', kind: 'all_off', device_states: [] })
}

// 场景执行（与 mockApi.executeScene 同语义：改开关态 + 记激活场景）。
function aiRunScene(spaceId, kind) {
  aiEnsureScenes(spaceId)
  const s = scenes.find((x) => x.space_id === spaceId && x.kind === kind)
  if (!s) return null
  const cmd = kind === 'all_on' ? 'on' : 'off'
  const results = []
  ;(devices[spaceId] || []).forEach((dev) => {
    if (dev.type !== 'sensor' && dev.type !== 'door') {
      switchStates[dev.device_id] = cmd
      results.push({ device_id: dev.device_id, command: cmd, status: 200 })
    }
  })
  activeSceneBySpace[spaceId] = s.scene_id
  return { scene: s, applied: results.length, results }
}

// 单设备开关（门禁走二次确认，不代开）。
function aiRunDevice(spaceId, deviceId, command) {
  const dev = (devices[spaceId] || []).find((d) => d.device_id === deviceId)
  if (!dev) return null
  if (dev.type === 'door') {
    return { device: dev, needConfirm: true }
  }
  switchStates[deviceId] = command
  if (spaceId) delete activeSceneBySpace[spaceId]
  return { device: dev, applied: true }
}

// 意图匹配 + 数据组装，产出 AI 回复正文。
function aiComposeReply(text) {
  const u = currentUser()
  if (!u) return '当前未登录，无法查询教室数据。'
  const t = String(text || '')
  const allowed = aiAccessibleSpaces()
  if (!allowed.length) return '当前账号没有可访问的教室，请先在「我的教室」里绑定教室。'

  // 提到但无权访问的空间 → 明确拒绝，不泄露数据。
  const mentioned = aiFindSpace(t)
  if (mentioned && !allowed.some((s) => s.space_id === mentioned.space_id)) {
    return `「${mentioned.name}」不在你的授权范围内，我无法查看该教室的数据。`
  }
  const space = aiFindSpace(t, allowed) || allowed[0]
  const spaceName = space.name
  const devList = (devices[space.space_id] || []).map((d) => toDeviceDoc(d, space.space_id))
  const typeName = aiSpaceTypeName(space.type)

  // 1) 执行场景 / 模式
  if (/(执行|运行|下发|启动|开启|打开)/.test(t) && /(模式|场景)/.test(t)) {
    const kind = /(离开|关闭|下课)/.test(t) ? 'all_off' : 'all_on'
    const r = aiRunScene(space.space_id, kind)
    if (!r) return `${spaceName}还没有可用的场景，请先在「设备控制」页创建。`
    const names = r.results
      .map((x) => {
        const d = (devices[space.space_id] || []).find((y) => y.device_id === x.device_id)
        return d ? d.label : x.device_id
      })
      .join('、')
    return `已在 ${spaceName} 执行「${r.scene.name}」，共下发 ${r.applied} 台设备：${names || '无'}。门禁与传感器不在场景控制范围内。`
  }

  // 2) 场景列表
  if (/(场景|模式)/.test(t)) {
    aiEnsureScenes(space.space_id)
    const list = scenes.filter((s) => s.space_id === space.space_id)
    if (!list.length) return `${spaceName}目前没有配置任何场景。`
    const lines = list.map((s, i) => `${i + 1}. ${s.name}（${s.kind === 'all_on' ? '全部开启' : s.kind === 'all_off' ? '全部关闭' : '自定义'}）`)
    return `${spaceName}共有 ${list.length} 个场景：\n${lines.join('\n')}`
  }

  // 3) 告警
  if (/(告警|报警|异常|离线提醒|报警记录)/.test(t)) {
    const list = alerts
      .filter((a) => u.role === 'admin' || allowed.some((s) => s.space_id === a.space_id))
      .slice()
      .sort((a, b) => (a.created_at < b.created_at ? 1 : -1))
      .slice(0, 5)
    if (!list.length) return '当前没有告警记录。'
    const lines = list.map((a, i) => {
      const sp = spaces.find((s) => s.space_id === a.space_id)
      const what = a.device_label || (AI_EVENT_TYPE[a.event_type] || a.event_type)
      const room = sp ? sp.name : '未知教室'
      return `${i + 1}. [${AI_ALERT_STATUS[a.status] || a.status}] ${what} · ${room} · ${aiAgo(a.created_at)}`
    })
    const unhandled = list.filter((a) => a.status === 'unhandled').length
    return `最近 ${list.length} 条告警（其中 ${unhandled} 条未处理）：\n${lines.join('\n')}`
  }

  // 4) 进出记录
  if (/(进出|通行|刷卡|人脸|考勤|记录)/.test(t)) {
    const list = ACCESS_RECORDS.filter((r) => u.role === 'admin' || allowed.some((s) => s.space_id === r.space_id))
      .slice()
      .sort((a, b) => (a.time < b.time ? 1 : -1))
      .slice(0, 8)
    if (!list.length) return '今天暂时没有进出记录。'
    const lines = list.map((r, i) => {
      const sp = spaces.find((s) => s.space_id === r.space_id)
      const room = sp ? sp.name : '未知教室'
      const res = r.result === 'login' ? '成功' : '拒绝'
      return `${i + 1}. ${r.time.slice(11, 16)} ${r.name} · ${room} · ${AI_AUTH_TYPE[r.auth_type] || r.auth_type} · ${res}`
    })
    const denied = list.filter((r) => r.result === 'denied').length
    return `今天的进出记录共 ${list.length} 条（拒绝 ${denied} 条），最近几条：\n${lines.join('\n')}`
  }

  // 5) 单设备开关（先于设备状态判定，否则「打开照明」会被当成状态查询）
  if (/(打开|开启|关闭|熄灭|启动|开灯|关灯)/.test(t)) {
    const cmd = /(关闭|熄灭|关灯|关)/.test(t) ? 'off' : 'on'
    const target = aiMatchDevice(devList, t)
    if (!target) {
      return `${spaceName}里没找到你说的设备。当前可选：${devList.map((d) => d.label).join('、')}。`
    }
    const r = aiRunDevice(space.space_id, target.device_id, cmd)
    if (r && r.needConfirm) {
      return `「${target.label}」属于门禁，出于安全我不能直接开门，请到「设备控制」页做二次确认。`
    }
    return `已把 ${spaceName} 的「${target.label}」设为${cmd === 'on' ? '开启' : '关闭'}。`
  }

  // 6) 设备状态
  if (/(设备|在线|离线|装置|摄像头|屏幕|照明|灯)/.test(t)) {
    const online = devList.filter((d) => d.status === 'online')
    const offline = devList.filter((d) => d.status !== 'online')
    const on = devList.filter((d) => d.app === 'on')
    let out = `${spaceName}（${typeName}）共 ${devList.length} 台设备：${online.length} 台在线、${offline.length} 台离线。`
    if (online.length) out += `\n在线：${online.map((d) => d.label).join('、')}`
    if (offline.length) out += `\n离线：${offline.map((d) => d.label).join('、')}`
    if (on.length) out += `\n当前已开启：${on.map((d) => d.label).join('、')}`
    return out
  }

  // 7) 教室 / 空间列表
  if (/(教室|空间|列表|有哪些|列出|能访问|我能)/.test(t)) {
    const lines = allowed.map((s, i) => `${i + 1}. ${s.name}（${aiSpaceTypeName(s.type)}）`)
    return `你当前可访问 ${allowed.length} 个教室：\n${lines.join('\n')}`
  }

  // 8) 兜底
  return [
    '我可以帮你做这些事：',
    '· 列出你能访问的教室',
    '· 查询某间教室的设备在线状态',
    '· 查看最近的告警',
    '· 查询今天的进出记录',
    '· 列出教室场景、执行「开启模式 / 离开模式」',
    '· 开关单台设备（门禁需在小程序里二次确认）',
    '试试问「A101 有哪些设备在线？」',
  ].join('\n')
}

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
    // 对齐 GET /api/miniapp/me：{user_id, role, name}（未绑定时 name 为空串）。
    return delay({ user_id: u.user_id, role: u.role, name: u.name || '' })
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
    // 按绑定顺序返回：USER_SPACES 的第一个即教师登录后的默认教室
    // （王老师默认落在「多媒体报告厅」，与网关单绑定时一致）。
    return delay(
      allowed
        .map((id) => spaces.find((s) => s.space_id === id && s.enabled))
        .filter(Boolean)
        .map((s) => ({ ...s }))
    )
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

  // 通知目标绑定：仅 admin。对齐 NotifyBindingService：
  // channel 必填且须在 wechat_miniapp|sms|dingtalk|wecom 内；GET 返回 {bindings:[...]}。
  notifyBindings(channel) {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    if (u.role !== 'admin') return Promise.reject(new Error('仅管理员可查看通知绑定'))
    if (!channel) return Promise.reject(new Error('channel required'))
    if (!KNOWN_CHANNELS.includes(channel)) return Promise.reject(new Error('unknown channel: ' + channel))
    const list = notifyBindings
      .filter((b) => b.channel === channel)
      .map((b) => ({
        user_id: b.user_id,
        channel: b.channel,
        external_id: b.external_id,
        updated_at: b.updated_at,
      }))
    return delay({ bindings: list })
  },

  saveNotifyBinding(payload) {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    if (u.role !== 'admin') return Promise.reject(new Error('仅管理员可管理通知绑定'))
    const channel = (payload && payload.channel) || ''
    const userId = (payload && payload.user_id) || ''
    const externalId = (payload && payload.external_id) || ''
    if (!KNOWN_CHANNELS.includes(channel)) return Promise.reject(new Error('unknown channel: ' + channel))
    if (!userId || !externalId) return Promise.reject(new Error('user_id and external_id are required'))
    const entry = {
      channel,
      user_id: userId,
      external_id: externalId,
      external_extra: (payload && payload.external_extra) || '',
      updated_at: new Date().toISOString(),
    }
    const idx = notifyBindings.findIndex((b) => b.channel === channel && b.user_id === userId)
    if (idx >= 0) notifyBindings[idx] = entry
    else notifyBindings.push(entry)
    return delay({ ok: true })
  },

  deleteNotifyBinding(channel, userId) {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    if (u.role !== 'admin') return Promise.reject(new Error('仅管理员可管理通知绑定'))
    if (!channel || !userId) return Promise.reject(new Error('channel and user_id are required'))
    const before = notifyBindings.length
    for (let i = notifyBindings.length - 1; i >= 0; i--) {
      if (notifyBindings[i].channel === channel && notifyBindings[i].user_id === userId) {
        notifyBindings.splice(i, 1)
      }
    }
    return delay({ ok: true, deleted: before - notifyBindings.length })
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
    // results[] = {device_id, command, status}（每设备下发 HTTP 状态，传输失败为 0），applied = results 长度。
    const results = []
    if (s.kind === 'custom') {
      s.device_states.forEach((x) => {
        switchStates[x.device_id] = x.command
        results.push({ device_id: x.device_id, command: x.command, status: 200 })
      })
    } else {
      const cmd = s.kind === 'all_on' ? 'on' : 'off'
      ;(devices[s.space_id] || []).forEach((dev) => {
        if (dev.type !== 'sensor' && dev.type !== 'door') {
          switchStates[dev.device_id] = cmd
          results.push({ device_id: dev.device_id, command: cmd, status: 200 })
        }
      })
    }
    const applied = results.length
    activeSceneBySpace[s.space_id] = sceneId
    return delay({ ok: true, scene_id: sceneId, space_id: s.space_id, applied, results, warning: '' })
  },

  // 订阅授权
  subscribe(payload) {
    if (!currentUser()) return Promise.reject(new Error('未登录'))
    return delay({ ok: true, ...payload })
  },

  getSubscriptions() {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    // 对齐 GET /api/miniapp/subscribe：{user_id, templates:{<tid>:{active,quota}}}
    return delay({ user_id: u.user_id, templates: {} })
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

  // ---- AI 助手（本地 mock，无需后端）----
  // 与网关 AiController 的返回结构一一对应：
  //   POST   /api/ai/chat           -> {reply, tool_calls, conversation_id}
  //   GET    /api/ai/conversations  -> {conversations:[{conversation_id,title,created_at,updated_at}]}
  //   GET    /api/ai/conversations/:id/messages -> {conversation_id, messages:[{role,content,created_at}]}
  //   DELETE /api/ai/conversations/:id -> {conversation_id, deleted:true}
  aiChat(messages, conversationId) {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    const lastUser = (messages || []).filter((m) => m.role === 'user').pop()
    const text = lastUser ? lastUser.content : ''
    let conv = aiConversations.find((c) => c.conversation_id === conversationId)
    if (!conv) {
      conv = {
        conversation_id: 'conv_' + ++aiConvSeq + '_' + Date.now(),
        user_id: u.user_id,
        title: (text || '新对话').slice(0, 16),
        created_at: new Date().toISOString(),
        updated_at: new Date().toISOString(),
        messages: [],
      }
      aiConversations.unshift(conv)
    }
    const now = new Date().toISOString()
    ;(messages || []).forEach((m) => {
      conv.messages.push({ role: m.role, content: m.content, created_at: now })
    })
    const reply = aiComposeReply(text)
    conv.messages.push({ role: 'assistant', content: reply, created_at: now })
    conv.updated_at = now
    return delay({ reply, tool_calls: [], conversation_id: conv.conversation_id }, 600)
  },

  aiConversations() {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    const list = aiConversations
      .filter((c) => c.user_id === u.user_id)
      .map((c) => ({
        conversation_id: c.conversation_id,
        title: c.title,
        created_at: c.created_at,
        updated_at: c.updated_at,
      }))
    return delay({ conversations: list }, 200)
  },

  aiMessages(conversationId) {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    const conv = aiConversations.find((c) => c.conversation_id === conversationId)
    if (!conv) {
      return Promise.reject(Object.assign(new Error('conversation not found'), { status: 404 }))
    }
    return delay(
      {
        conversation_id: conv.conversation_id,
        messages: conv.messages.map((m) => ({ ...m })),
      },
      200
    )
  },

  aiDeleteConversation(conversationId) {
    const u = currentUser()
    if (!u) return Promise.reject(new Error('未登录'))
    const idx = aiConversations.findIndex((c) => c.conversation_id === conversationId)
    if (idx < 0) {
      return Promise.reject(Object.assign(new Error('conversation not found'), { status: 404 }))
    }
    aiConversations.splice(idx, 1)
    return delay({ conversation_id: conversationId, deleted: true }, 200)
  },
}
