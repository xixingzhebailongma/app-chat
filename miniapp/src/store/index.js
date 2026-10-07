import { reactive } from 'vue'
import { getSession, setSession, clearSession } from '../utils/auth'

// 轻量全局会话（避免额外依赖 Pinia）：登录态 + 未处理告警数（供 tabbar 红点）。
const session = getSession()

export const store = reactive({
  token: session.token || '',
  userId: session.userId || '',
  role: session.role || '',
  name: session.name || '',
  unhandledAlerts: 0,
  pendingAlertId: '', // 订阅消息深链待跳转的 alert_id（登录后消费）
})

export function isLoggedIn() {
  return !!store.token
}

export function applySession(s) {
  store.token = s.token || ''
  store.userId = s.userId || s.user_id || ''
  store.role = s.role || ''
  store.name = s.name || ''
  setSession({
    token: store.token,
    userId: store.userId,
    role: store.role,
    name: store.name,
  })
}

export function logout() {
  clearSession()
  store.token = ''
  store.userId = ''
  store.role = ''
  store.name = ''
  store.unhandledAlerts = 0
  store.pendingAlertId = ''
}
