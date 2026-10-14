/**
 * 统一请求封装：所有接口返回 { data } 或 { error: { code, message } }
 * - 自动注入 Authorization: Bearer {token}
 * - 401 统一清理登录态并跳登录页
 * - 网络失败返回可读文案
 */
import { API_BASE, REQUEST_TIMEOUT } from './config'
import { store, clearAuth } from '../store'

function toLogin() {
  clearAuth()
  uni.reLaunch({ url: '/pages/login/login' })
}

function networkMessage() {
  if (!store.netOnline) return '网络已断开，请检查网络连接'
  return '网络请求失败，请稍后重试'
}

export function request({ url, method = 'GET', data, auth = true }) {
  return new Promise((resolve) => {
    if (!store.netOnline) {
      return resolve({ error: { code: -2, message: networkMessage() } })
    }
    const header = { 'Content-Type': 'application/json' }
    if (auth && store.token) header.Authorization = 'Bearer ' + store.token

    uni.request({
      url: API_BASE + url,
      method,
      data,
      header,
      timeout: REQUEST_TIMEOUT,
      success: (res) => {
        if (res.statusCode === 401) {
          toLogin()
          return resolve({ error: { code: 401, message: '登录已过期，请重新登录' } })
        }
        if (res.statusCode >= 200 && res.statusCode < 300) {
          return resolve({ data: res.data })
        }
        const msg =
          (res.data && (res.data.message || res.data.error)) ||
          `请求失败（${res.statusCode}）`
        resolve({ error: { code: res.statusCode, message: msg } })
      },
      fail: (err) => {
        resolve({ error: { code: -1, message: networkMessage() } })
      },
    })
  })
}

export function toastError(error) {
  if (!error) return
  uni.showToast({ title: error.message || '操作失败', icon: 'none' })
}
