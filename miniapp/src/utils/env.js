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

// 解析传感器 app，返回 { temperature, humidity, co2 }（单位：℃ / % / ppm），
// 任一值缺失时为 null；无 app / 非传感器 / 解析失败时返回 null。
export function parseSensorApp(d) {
  if (!d || typeof d.app !== 'string' || !d.app) return null
  let o
  try {
    o = JSON.parse(d.app)
  } catch (e) {
    return null
  }
  if (!o || typeof o !== 'object') return null
  const env = {
    temperature: toNum(o.temperature),
    humidity: toNum(o.humidity),
    co2: toNum(o.co2),
  }
  // 三项全空视作无环境数据
  if (env.temperature == null && env.humidity == null && env.co2 == null) {
    return null
  }
  return env
}

// 该设备是否携带可展示的环境数据（用于筛选传感器设备）。
export function hasEnvData(d) {
  return parseSensorApp(d) !== null
}
