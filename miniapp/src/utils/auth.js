// 会话持久化：token + 用户信息存本地，重启后仍保持登录态。
const KEY = 'session'

export function getSession() {
  try {
    return uni.getStorageSync(KEY) || {}
  } catch (e) {
    return {}
  }
}

export function getToken() {
  return getSession().token || ''
}

export function setSession(session) {
  try {
    uni.setStorageSync(KEY, session)
  } catch (e) {}
}

export function clearSession() {
  try {
    uni.removeStorageSync(KEY)
  } catch (e) {}
}
