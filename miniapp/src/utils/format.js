// 展示层小工具

// 设备类型 → 中文名（综合屏 API 契约：device_type ∈ zigbee / fuhe-*，
// zigbee 子设备再按 zb_type 区分 switch/sensor）。
const DEVICE_TYPE_LABELS = {
  'fuhe-screen': '综合屏',
  'fuhe-board': '电子班牌',
  'fuhe-encoder': '编码器',
  'fuhe-decoder': '解码器',
}

export function deviceTypeLabel(device_type, zb_type) {
  if (device_type === 'zigbee') {
    return zb_type === 'sensor' ? '传感器' : '开关'
  }
  return DEVICE_TYPE_LABELS[device_type] || device_type || '设备'
}

// 空间类型 → 中文名（第一级分组）
export const SPACE_TYPES = {
  standard: '标准教室',
  lecture: '多媒体报告厅',
  office: '办公室',
  gym: '体育馆',
  lab: '实验室',
  library: '图书馆',
  canteen: '食堂',
}

export function spaceTypeLabel(type) {
  return SPACE_TYPES[type] || type || '空间'
}

// 空间类型的展示顺序（已知类型）
export const SPACE_TYPE_ORDER = ['standard', 'lecture', 'office', 'gym', 'lab', 'library', 'canteen']

// 把一批空间类型按「已知顺序在前、未知类型按字母序在后」排序。
// 这样以后新增类型（如体育馆）只需在 SPACE_TYPES 里加标签；即便不加，
// 未知类型也会被兜底展示（label 回退到 type 本身），不会整块消失。
export function orderedSpaceTypeKeys(types) {
  const seen = [...new Set(types)]
  const known = SPACE_TYPE_ORDER.filter((t) => seen.includes(t))
  const rest = seen.filter((t) => !SPACE_TYPE_ORDER.includes(t)).sort()
  return known.concat(rest)
}

// 告警类型 → 中文名 + 语义色
export const EVENT_TYPES = {
  device_offline: { label: '设备离线', tone: 'danger' },
  sensor_threshold: { label: '传感器超阈值', tone: 'warning' },
  face_login_failed: { label: '人脸识别失败', tone: 'info' },
}

export function eventTypeLabel(type) {
  return (EVENT_TYPES[type] && EVENT_TYPES[type].label) || type || '未知告警'
}

export function eventTypeTone(type) {
  return (EVENT_TYPES[type] && EVENT_TYPES[type].tone) || 'info'
}

// 告警状态 → 中文名 + 语义色
export const ALERT_STATUS = {
  unhandled: { label: '待处理', tone: 'warning' },
  handling: { label: '处理中', tone: 'neutral' },
  resolved: { label: '已处理', tone: 'neutral' },
}

export function alertStatusLabel(status) {
  return (ALERT_STATUS[status] && ALERT_STATUS[status].label) || status || '未知'
}

export function alertStatusTone(status) {
  return (ALERT_STATUS[status] && ALERT_STATUS[status].tone) || 'neutral'
}

// 时间戳/字符串 → 「MM-DD HH:mm」，容错：空值返回 ''
export function formatTime(t) {
  if (!t) return ''
  const d = typeof t === 'number' ? new Date(t) : new Date(String(t).replace(/-/g, '/'))
  if (isNaN(d.getTime())) return ''
  const p = (n) => (n < 10 ? '0' + n : '' + n)
  return `${d.getMonth() + 1}-${p(d.getDate())} ${p(d.getHours())}:${p(d.getMinutes())}`
}
