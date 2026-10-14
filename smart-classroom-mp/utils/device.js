/** 设备展示辅助：类型名 / 图标 / 分类 */
import { fromNow } from './time'
import { store } from '../store'

const TYPE_NAMES = {
  zigbee: 'Zigbee 设备',
  'fuhe-screen': '综合屏',
  'fuhe-board': '融合板卡',
  'fuhe-encoder': '编解码器',
}

export function typeName(d) {
  return TYPE_NAMES[d.device_type] || d.device_type || ''
}

/** 开关类设备（app 为 'on'/'off'），含 fuhe 类 */
export function isSwitchDevice(d) {
  return d && (d.app === 'on' || d.app === 'off')
}

/** 传感器类设备（app 为 JSON 字符串） */
export function isSensorDevice(d) {
  return d && d.app !== 'on' && d.app !== 'off' && typeof d.app === 'string' && d.app.trim().startsWith('{')
}

export function isOffline(d) {
  return !d || d.status !== 'online'
}

/** 设备 → app-icon 图标键（按名称/类型推断） */
export function iconKeyOf(d) {
  const l = (d.label || '') + (d.device_id || '')
  if (/灯|照明/.test(l)) return 'light'
  if (/空调/.test(l)) return 'ac'
  if (/门/.test(l)) return 'lock'
  if (/投影|幕/.test(l)) return 'projector'
  if (/传感器|sensor|感应/.test(l)) return 'sensor'
  if (d.device_type === 'fuhe-screen') return 'screen'
  return 'codec'
}

/**
 * 设备 → device-row 视图模型
 * status（网络）与 app（开关）两个独立维度分开传递：
 * online/state/on 分别对应行样式、开关状态、开关值
 */
export function toDeviceRow(d) {
  void store.tick // 依赖 tick：副信息里的相对时间随全局刷新信号重算
  const p = store.pendingCmds[d.device_id]
  const sub = [typeName(d)]
  if (d.last_seen) sub.push('上报 ' + fromNow(d.last_seen))
  return {
    device_id: d.device_id,
    device_type: d.device_type,
    iconType: iconKeyOf(d),
    label: d.label || d.device_id,
    name: d.label,
    online: d.status === 'online',
    state: (d.app === 'on' || d.app === 'off') ? d.app : undefined,
    on: d.app === 'on',
    pending: !!p,
    sub: sub.join(' · '),
    _raw: d, // 原始设备对象：控制指令下发用（含 device_type / app 等）
  }
}
