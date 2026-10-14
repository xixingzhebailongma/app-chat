/**
 * 环境指标与设备类型的视觉主题（JS 侧数据源）
 * 色板对齐 iot-mp-prompts/design.md 第 2.4 节设备分类点缀体系：
 * 每个指标/分类 = 点缀色 + 成对浅底（小图标底块/小色块专用，禁止大面积铺色）。
 */

/** 环境指标 → 点缀色 + 浅底（温度暖橙 / 湿度蓝 / CO₂ 紫 / 光照黄 / PM2.5 绿 / 人体感应紫） */
export const METRIC_THEME = {
  temperature: { label: '温度',     color: '#FF9500', soft: '#FFF4E5', unit: '℃',    icon: 'temperature' },
  humidity:    { label: '湿度',     color: '#007AFF', soft: '#E5F1FF', unit: '%RH',  icon: 'humidity' },
  co2:         { label: 'CO₂',      color: '#5856D6', soft: '#ECECFB', unit: 'ppm',  icon: 'co2' },
  light:       { label: '光照',     color: '#FFCC00', soft: '#FFF8D6', unit: 'lux',  icon: 'sunlight' },
  pm25:        { label: 'PM2.5',    color: '#34C759', soft: '#E9F9EE', unit: 'μg/m³', icon: 'pm' },
  motion:      { label: '人体感应', color: '#AF52DE', soft: '#F4ECFD', unit: '',     icon: 'motion' },
}

/** 设备类型 → 图标（含历史字段别名） */
export const DEVICE_ICONS = {
  screen: 'screen', display: 'screen', all_in_one: 'screen',
  light: 'bulb', lamp: 'bulb',
  ac: 'ac', aircon: 'ac', air_conditioner: 'ac',
  projector: 'projector',
  sensor: 'sensor',
  lock: 'lock', door_lock: 'lock',
  codec: 'codec', decoder: 'codec',
}

export function deviceIcon(type) {
  return DEVICE_ICONS[type] || 'codec'
}

/** 设备图标 → 分类（design.md：照明/环境/安防/影音/插座/传感器/其他） */
const ICON_CAT = {
  screen: 'media',
  projector: 'media',
  bulb: 'light',
  ac: 'climate',
  lock: 'security',
  sensor: 'sensor',
  codec: 'plug',
}

const CAT_THEME = {
  media:    { color: '#AF52DE', soft: '#F4ECFD' },
  light:    { color: '#FFCC00', soft: '#FFF8D6' },
  climate:  { color: '#34C759', soft: '#E9F9EE' },
  security: { color: '#FF3B30', soft: '#FFEDEC' },
  sensor:   { color: '#5856D6', soft: '#ECECFB' },
  plug:     { color: '#007AFF', soft: '#E5F1FF' },
  other:    { color: '#8E8E93', soft: '#F2F2F7' },
}

/** 图标名 → { color, soft }；离线态由调用方置灰（soft 不生效） */
export function deviceCat(icon) {
  return CAT_THEME[ICON_CAT[icon] || 'other']
}

/** hex 色值转 rgba（需要临时透明度时使用，优先使用成对浅底） */
export function hexToRgba(hex, alpha) {
  const h = (hex || '#000000').replace('#', '')
  const full = h.length === 3 ? h.split('').map((c) => c + c).join('') : h
  const r = parseInt(full.slice(0, 2), 16)
  const g = parseInt(full.slice(2, 4), 16)
  const b = parseInt(full.slice(4, 6), 16)
  return `rgba(${r}, ${g}, ${b}, ${alpha})`
}
