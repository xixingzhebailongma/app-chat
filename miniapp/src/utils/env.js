// 传感器环境数据：设备的 app 字段是 JSON 字符串，形如
// {"temperature":25.1,"humidity":60,"co2":800}，由边侧 go-backend 透传而来
// （见 wechat-gateway/scripts/mock-go-backend.py 与 api/mock.js 的 sensor 设备）。

export function toNum(v) {
  if (typeof v === 'number') return isFinite(v) ? v : null
  if (typeof v === 'string') {
    const s = v.trim()
    if (s === '') return null
    const n = Number(s)
    return isFinite(n) ? n : null
  }
  return null
}

// 解析传感器 app，返回全部已识别指标的数值对象
// （temperature/humidity/co2/light/pm25/pir，单位 ℃/%RH/ppm/lux/μg·m⁻³/布尔）；
// 无 app / 非传感器 / 解析失败 / 全部字段缺失时返回 null。
const METRIC_KEYS = ['temperature', 'humidity', 'co2', 'light', 'pm25', 'pir']

export function parseSensorApp(d) {
  if (!d || typeof d.app !== 'string' || !d.app) return null
  let o
  try {
    o = JSON.parse(d.app)
  } catch (e) {
    return null
  }
  if (!o || typeof o !== 'object') return null
  const env = {}
  METRIC_KEYS.forEach((k) => {
    if (o[k] === undefined || o[k] === null) return
    if (k === 'pir') {
      env[k] = typeof o[k] === 'boolean' ? o[k] : o[k] === 'true' || o[k] === 1 || o[k] === '1'
      return
    }
    const n = toNum(o[k])
    if (n != null) env[k] = n
  })
  // 全部指标缺失视作无环境数据
  if (!Object.keys(env).length) return null
  return env
}

// 该设备是否携带可展示的环境数据（用于筛选传感器设备）。
export function hasEnvData(d) {
  return parseSensorApp(d) !== null
}
