import { getToken } from './auth'
import { logout } from '../store/index'

// 从多种错误体里提取可读文案：网关标准体 {error:{code,message}}、
// 边侧 go-backend 的 {detail}（401）/ {message}（400）/ 字符串 {error}。
function extractError(data) {
  if (!data) return null
  if (data.error && typeof data.error === 'object' && data.error.message) return data.error.message
  if (typeof data.detail === 'string') return data.detail
  if (typeof data.message === 'string') return data.message
  if (typeof data.error === 'string') return data.error
  return null
}

// 提取机器可读错误码（仅网关标准体有 code，如 GO_BACKEND_UNAVAILABLE）。
function extractCode(data) {
  if (data && data.error && typeof data.error === 'object') return data.error.code
  return undefined
}

// 登录/绑定是「换 token」的前置接口：不携带 access token，其 401 表示
// 登录失败（code 无效 / 密码错）而非会话过期，须排除在 401 拦截之外。
function isAuthEndpoint(url) {
  return /\/api\/miniapp\/(login|bind)\/?$/.test(url)
}

// 401 防抖标记：同一会话里多个并发请求同时 401，只清会话 + 跳登录一次。
let redirectingToLogin = false

// 统一请求封装：自动带 JWT、把网关的 {"error":{code,message}} 转成可读错误，
// 并在非登录接口返回 401 时清会话踢回登录。
export function request(options) {
  return new Promise((resolve, reject) => {
    const header = Object.assign({ 'Content-Type': 'application/json' }, options.header || {})
    const authEndpoint = isAuthEndpoint(options.url)
    const token = getToken()
    // 登录/绑定不携带 access token（避免残留旧 token 被误带）。
    if (token && !authEndpoint) {
      header.Authorization = 'Bearer ' + token
    }
    uni.request({
      url: options.url,
      method: options.method || 'GET',
      data: options.data || {},
      header,
      timeout: options.timeout || 10000,
      success: (res) => {
        const status = res.statusCode
        if (status >= 200 && status < 300) {
          // 有请求成功说明 token 仍有效：复位防抖标记（支持「重新登录后再次过期」）。
          redirectingToLogin = false
          resolve(res.data)
          return
        }
        // 会话过期：非登录接口返回 401 → 清会话跳登录（登录页 401 不拦截）。
        if (status === 401 && !authEndpoint && !redirectingToLogin) {
          redirectingToLogin = true
          logout()
          uni.reLaunch({ url: '/pages/login/login' })
        }
        // 统一中文文案：403 无权限 / 503 功能未上线，其余回退后端 message。
        // 具体端点可用 err.code 覆盖（见 controlError.js / alertError.js），
        // err.code / err.status 仍保留供调用方程序化处理。
        let msg
        if (status === 403) {
          msg = '无权限操作'
        } else if (status === 503) {
          msg = '功能暂未上线'
        } else {
          msg = extractError(res.data) || '请求失败 (' + status + ')'
        }
        const err = new Error(msg)
        err.code = extractCode(res.data)
        err.status = status
        reject(err)
      },
      fail: (err) => reject(new Error(err.errMsg || '网络异常，请检查连接')),
    })
  })
}
