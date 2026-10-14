/**
 * 时间工具：后端 last_seen / start / end 均为 UTC ISO 8601，
 * 统一在此转换为本地时间；相对时间展示依赖 store.tick 触发重渲染。
 */

export function parseUtc(s) {
  if (!s) return null
  if (s instanceof Date) return s
  if (typeof s === 'number') return new Date(s)
  let str = String(s)
  // 无时区标记的 ISO 串按 UTC 处理
  if (!/Z$|[+-]\d{2}:?\d{2}$/.test(str)) str += 'Z'
  const d = new Date(str)
  return isNaN(d.getTime()) ? null : d
}

function pad(n) { return n < 10 ? '0' + n : '' + n }

export function fmtClock(d = new Date()) {
  return pad(d.getHours()) + ':' + pad(d.getMinutes())
}

export function fmtClockSec(d = new Date()) {
  return pad(d.getHours()) + ':' + pad(d.getMinutes()) + ':' + pad(d.getSeconds())
}

export function fmtDateTime(s) {
  const d = parseUtc(s)
  if (!d) return ''
  const now = new Date()
  const y = d.getFullYear() === now.getFullYear() ? '' : d.getFullYear() + '年'
  return `${y}${d.getMonth() + 1}月${d.getDate()}日 ${pad(d.getHours())}:${pad(d.getMinutes())}`
}

export function fmtDateCN(s) {
  const d = parseUtc(s)
  if (!d) return ''
  return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())}`
}

/** 相对时间：刚刚 / N分钟前 / N小时前 / 昨天 / N天前 / M月D日 */
export function fromNow(s) {
  const d = parseUtc(s)
  if (!d) return ''
  const diff = Date.now() - d.getTime()
  if (diff < 0) return fmtClock(d)
  const m = Math.floor(diff / 60000)
  if (m < 1) return '刚刚'
  if (m < 60) return m + ' 分钟前'
  const h = Math.floor(m / 60)
  if (h < 24) return h + ' 小时前'
  const day = Math.floor(h / 24)
  if (day === 1) return '昨天'
  if (day < 7) return day + ' 天前'
  return `${d.getMonth() + 1}月${d.getDate()}日`
}

/** 今天是周几：1=周一 … 7=周日（与课表 weekday 定义一致，注意不是 0=周日） */
export function weekdayToday() {
  const day = new Date().getDay()
  return day === 0 ? 7 : day
}

export const WEEKDAY_NAMES = ['', '周一', '周二', '周三', '周四', '周五', '周六', '周日']
