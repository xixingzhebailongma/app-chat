import { reactive } from 'vue'
import { getSession, setSession, clearSession } from '../utils/auth'

// 轻量全局会话（避免额外依赖 Pinia）：登录态 + 未处理告警数（供 tabbar 红点）。
const session = getSession()

// AI 会话 id 单独持久化（跨刷新/跨设备续聊）。
const AI_CONV_KEY = 'aiConversationId'

function readAiConversationId() {
  try {
    return uni.getStorageSync(AI_CONV_KEY) || ''
  } catch (e) {
    return ''
  }
}

export const store = reactive({
  token: session.token || '',
  userId: session.userId || '',
  role: session.role || '',
  name: session.name || '',
  unhandledAlerts: 0,
  pendingAlertId: '', // 订阅消息深链待跳转的 alert_id（登录后消费）
  aiDraft: '',
  aiConversationId: readAiConversationId(),
  aiMessages: [],
})

export function saveAiConversationId(id) {
  store.aiConversationId = id || ''
  try {
    if (id) uni.setStorageSync(AI_CONV_KEY, id)
    else uni.removeStorageSync(AI_CONV_KEY)
  } catch (e) {}
}

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
  store.aiDraft = ''
  store.aiMessages = []
  saveAiConversationId('')
}
