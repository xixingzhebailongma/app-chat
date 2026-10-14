/**
 * 传感器字段元数据 & 解析
 * 关键约束：app 为 JSON 字符串时只渲染实际存在的字段，
 * 设备不支持的指标不占位、不显示"—"或 0。
 */

/* icon 为 app-icon 图标名（与 config/metrics.js 的 METRIC_THEME 对齐，不用 emoji） */
export const SENSOR_META = {
  temperature: { label: '温度', unit: '℃', icon: 'temperature', decimals: 1 },
  humidity: { label: '湿度', unit: '%RH', icon: 'humidity', decimals: 0 },
  co2: { label: 'CO₂', unit: 'ppm', icon: 'co2', decimals: 0 },
  light: { label: '光照', unit: 'lux', icon: 'sunlight', decimals: 0 },
  pm25: { label: 'PM2.5', unit: 'μg/m³', icon: 'pm', decimals: 0 },
  pir: { label: '人体感应', unit: '', icon: 'motion', boolean: true },
}

/** 解析设备 app 字段：开关类返回 null；传感器类返回对象 */
export function parseApp(device) {
  if (!device || device.app === undefined || device.app === null) return null
  const app = device.app
  if (app === 'on' || app === 'off') return null
  if (typeof app === 'string') {
    try { return JSON.parse(app) } catch (e) { return null }
  }
  if (typeof app === 'object') return app
  return null
}

/** 解析 fuhe 类设备 info 运行指标 */
export function parseInfo(device) {
  try {
    if (!device || !device.info) return null
    return typeof device.info === 'string' ? JSON.parse(device.info) : device.info
  } catch (e) { return null }
}

function levelOf(key, v) {
  if (v === null || v === undefined) return 'none'
  const n = Number(v)
  if (Number.isNaN(n)) return 'none'
  switch (key) {
    case 'temperature': return n >= 30 ? 'danger' : n >= 28 ? 'warn' : 'ok'
    case 'humidity': return (n < 30 || n > 70) ? 'warn' : 'ok'
    case 'co2': return n >= 1500 ? 'danger' : n >= 1000 ? 'warn' : 'ok'
    case 'pm25': return n >= 75 ? 'danger' : n >= 35 ? 'warn' : 'ok'
    default: return 'ok'
  }
}

/** 聚合多个传感器设备 → 环境读数列表（只含实际存在且识别的字段） */
export function envReadings(devices = []) {
  const merged = {}
  ;(devices || []).forEach((d) => {
    const o = parseApp(d)
    if (o) Object.keys(o).forEach((k) => { if (SENSOR_META[k]) merged[k] = o[k] })
  })
  // 按 SENSOR_META 声明顺序输出，且只输出存在的字段
  return Object.keys(SENSOR_META)
    .filter((k) => k in merged)
    .map((k) => {
      const meta = SENSOR_META[k]
      return {
        key: k,
        label: meta.label,
        unit: meta.unit,
        icon: meta.icon,
        decimals: meta.decimals,
        boolean: meta.boolean,
        value: merged[k],
        level: levelOf(k, merged[k]),
      }
    })
}

export function fmtSensorValue(item) {
  if (item.boolean) return item.value ? '有人' : '无人'
  const n = Number(item.value)
  if (Number.isNaN(n)) return String(item.value)
  return n.toFixed(item.decimals || 0)
}
