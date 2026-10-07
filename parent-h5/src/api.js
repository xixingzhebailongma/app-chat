import config from './config'
import { mockApi } from './mock'

// 从多种错误体提取可读文案（与 miniapp/utils/request.js 同款解析）。
function extractError(data) {
  if (!data) return null
  if (data.error && typeof data.error === 'object' && data.error.message) return data.error.message
  if (typeof data.detail === 'string') return data.detail
  if (typeof data.message === 'string') return data.message
  if (typeof data.error === 'string') return data.error
  return null
}

const REQUEST_TIMEOUT_MS = 15000

let onUnauthorized = null
let reauthing = false
export function setUnauthorizedHandler(fn) {
  onUnauthorized = fn
}

// 401 统一触发重新授权；并发 401 只触发一次（reauthing 节流）。
function maybeReauth() {
  if (reauthing || typeof onUnauthorized !== 'function') return
  reauthing = true
  try {
    onUnauthorized()
  } finally {
    // 跳转是同步 location.href；若未真正离开（如 appid 未配），短暂后复位允许重试。
    setTimeout(() => { reauthing = false }, 1000)
  }
}

function request(url, method, data) {
  const controller = new AbortController()
  const timer = setTimeout(() => controller.abort(), REQUEST_TIMEOUT_MS)
  return fetch(url, {
    method,
    headers: { 'Content-Type': 'application/json' },
    body: method === 'GET' ? undefined : JSON.stringify(data || {}),
    signal: controller.signal,
  }).then(async (res) => {
    const body = await res.json().catch(() => null)
    if (res.ok) return body
    if (res.status === 401) maybeReauth()
    const err = new Error(extractError(body) || '请求失败 (' + res.status + ')')
    err.status = res.status
    throw err
  }).catch((e) => {
    if (e && e.name === 'AbortError') {
      const err = new Error('请求超时，请稍后重试')
      err.status = 0
      throw err
    }
    if (e instanceof TypeError) {
      // fetch 网络层失败（断网 / DNS / 证书 / CORS 等）→ 友好文案，不再直露 "Failed to fetch"。
      const err = new Error('网络异常，请检查网络后重试')
      err.status = 0
      throw err
    }
    throw e
  }).finally(() => clearTimeout(timer))
}

const base = () => config.OA_BASE_URL

export const api = {
  // 网页授权：code 换 openid + openid_token
  authorize(code) {
    if (config.USE_MOCK) return mockApi.authorize(code)
    return request(base() + '/api/oa/bind/authorize?code=' + encodeURIComponent(code), 'GET')
  },
  // 发送短信验证码
  sendCode(phone) {
    if (config.USE_MOCK) return mockApi.sendCode(phone)
    return request(base() + '/api/oa/bind/send-code', 'POST', { phone })
  },
  // 绑定确认
  confirm(openidToken, studentNo, phone, smsCode) {
    if (config.USE_MOCK) return mockApi.confirm(openidToken, studentNo, phone, smsCode)
    return request(base() + '/api/oa/bind/confirm', 'POST', {
      openid_token: openidToken,
      student_no: studentNo,
      phone,
      sms_code: smsCode,
    })
  },
  // 查当前 openid 的绑定列表（多孩子）。
  me(openidToken) {
    if (config.USE_MOCK) return mockApi.me(openidToken)
    return request(base() + '/api/oa/bind/me?openid_token=' + encodeURIComponent(openidToken), 'GET')
  },
  // 删除指定学号的绑定。
  unbind(openidToken, studentNo) {
    if (config.USE_MOCK) return mockApi.unbind(openidToken, studentNo)
    return request(base() + '/api/oa/bind/unbind', 'POST', {
      openid_token: openidToken,
      student_no: studentNo,
    })
  },
}
