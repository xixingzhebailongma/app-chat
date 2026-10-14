/**
 * 告警数据 → 告警卡片(alarm-card)视图模型的映射
 * 推送分级原因（抑制/降级）如实标注为 notes，解释"为什么没收到通知"。
 */
import { fromNow } from './time'
import { store } from '../store'

const TYPE_ICONS = {
  sensor_threshold: 'temperature',
  device_offline: 'sensor',
  security: 'warning',
  system: 'info',
}

/**
 * @param {Object} a 后端告警 { id, type, status, title, space_name, summary, occurred_at, suppress_reason, degrade_note }
 * @returns alarm-card 需要的 { status, type, title, room, desc, time, notes, raw }
 */
export function toAlarmCard(a) {
  const notes = []
  if (a.suppress_reason) notes.push({ text: a.suppress_reason, tone: 'gray' })
  if (a.degrade_note) notes.push({ text: a.degrade_note, tone: 'orange' })
  void store.tick // 依赖 tick：相对时间随全局刷新信号重算
  return {
    id: a.id,
    status: a.status === 'pending' ? 'unhandled' : (a.status || 'unhandled'),
    type: TYPE_ICONS[a.type] || 'warning',
    title: a.title || '告警',
    room: a.space_name || '',
    desc: a.summary || '',
    time: fromNow(a.occurred_at),
    notes,
    raw: a,
  }
}
