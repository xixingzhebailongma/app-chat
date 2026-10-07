// 图标映射（uni-icons 的 type 名）。
// uni-icons 无专用设备图标，取语义最接近的线性图标；颜色统一由组件用 color 传入（中性单色），
// 不用彩色区分，语义色只留给 StatusTag 承载。

// 底部 tab 图标
export const TAB_ICONS = {
  home: 'home', // 首页
  classroom: 'location', // 我的教室
  devices: 'gear', // 设备
  alerts: 'notification', // 告警
  profile: 'person', // 我的
}

// 设备类型 → 图标（综合屏 API 契约：device_type ∈ zigbee / fuhe-*，
// zigbee 子设备再按 zb_type 区分 switch/sensor）
const DEVICE_TYPE_ICONS = {
  'fuhe-screen': 'videocam', // 综合屏：显示/屏幕
  'fuhe-board': 'flag', // 电子班牌：标识牌
  'fuhe-encoder': 'settings',
  'fuhe-decoder': 'settings',
}

// 告警类型 → 图标
export const EVENT_TYPE_ICONS = {
  device_offline: 'closeempty', // 设备离线：断连
  sensor_threshold: 'fire', // 传感器超阈值：触发/异常
  face_login_failed: 'eye', // 人脸识别失败：视觉/识别
}

export function deviceTypeIcon(device_type, zb_type) {
  if (device_type === 'zigbee') {
    return zb_type === 'sensor' ? 'scan' : 'star'
  }
  return DEVICE_TYPE_ICONS[device_type] || 'settings'
}

export function eventTypeIcon(type) {
  return EVENT_TYPE_ICONS[type] || 'notification'
}
