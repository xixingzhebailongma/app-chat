/**
 * WebSocket 连接管理
 * - token 通过 URL 传递（WebSocket 不支持自定义 Header）
 * - 服务端每 30s 发 ping 保活：客户端忽略，但用于死链检测（75s 未收到任何消息强制重连）
 * - 断线自动重连：指数退避 + 随机抖动（1s→2s→4s…上限 30s）
 * - 重连成功后广播 resync：各页面重新拉全量快照对账
 * - 服务端在客户端消费过慢、发送缓冲区满时会主动断开 → 重连逻辑同样覆盖此场景
 */
import { WS_BASE, USE_MOCK } from '../api/config'
import { createMockWs } from '../api/mock'
import { store } from '../store'

let socket = null
let handlers = {}
let retries = 0
let reconnectTimer = null
let watchdogTimer = null
let lastMsgAt = 0
let currentSpaceId = ''
let manualClose = false

export function onWs(type, fn) {
  (handlers[type] = handlers[type] || []).push(fn)
  return () => offWs(type, fn)
}

export function offWs(type, fn) {
  const arr = handlers[type]
  if (!arr) return
  const i = arr.indexOf(fn)
  if (i > -1) arr.splice(i, 1)
}

function emit(type, payload) {
  ;(handlers[type] || []).slice().forEach((fn) => {
    try { fn(payload) } catch (e) { console.error('[ws] handler error', e) }
  })
}

/** 建立连接（切换教室时传新 spaceId 会重建连接） */
export function connectWs(spaceId = '') {
  currentSpaceId = spaceId || ''
  manualClose = false
  teardown()
  open()
}

/** onShow / 网络恢复时确保连接存活 */
export function ensureWs() {
  if (!store.token || manualClose) return
  if (!socket || store.wsStatus === 'offline' || store.wsStatus === 'reconnecting') open()
}

export function closeWs() {
  manualClose = true
  teardown()
  store.wsStatus = 'offline'
}

function teardown() {
  clearTimeout(reconnectTimer)
  clearInterval(watchdogTimer)
  if (socket) {
    const s = socket
    socket = null
    try { s.close && s.close({}) } catch (e) { /* noop */ }
  }
}

function open() {
  if (!store.token) return
  store.wsStatus = 'connecting'
  const url = `${WS_BASE}?token=${encodeURIComponent(store.token)}&space_id=${encodeURIComponent(currentSpaceId)}`

  if (USE_MOCK) {
    socket = createMockWs(url, onMessage, onOpen, onClose)
    return
  }

  const task = uni.connectSocket({
    url,
    fail: () => { if (!manualClose) scheduleReconnect() },
  })
  socket = task
  task.onOpen(() => onOpen())
  task.onMessage((res) => {
    try { onMessage(JSON.parse(res.data)) } catch (e) { /* 非 JSON 消息忽略 */ }
  })
  task.onClose(() => { if (socket === task) { socket = null; onClose() } })
  task.onError(() => { if (socket === task) { socket = null; onClose() } })
}

function onOpen() {
  retries = 0
  lastMsgAt = Date.now()
  store.wsStatus = 'online'
  startWatchdog()
  // 重连成功 → 通知各页面重新拉全量快照对账
  emit('resync', { space_id: currentSpaceId })
  uni.$emit('ws:resync', { space_id: currentSpaceId })
}

function onClose() {
  if (manualClose) { store.wsStatus = 'offline'; return }
  store.wsStatus = 'reconnecting'
  scheduleReconnect()
}

function onMessage(msg) {
  lastMsgAt = Date.now()
  if (!msg || !msg.type) return
  if (msg.type === 'ping') return // 服务端心跳，忽略
  const payload = msg.payload !== undefined ? msg.payload : (msg.data !== undefined ? msg.data : msg)
  emit(msg.type, payload)
}

function scheduleReconnect() {
  if (manualClose) return
  const delay = Math.min(30000, 1000 * Math.pow(2, retries)) + Math.floor(Math.random() * 500)
  retries++
  clearTimeout(reconnectTimer)
  reconnectTimer = setTimeout(() => { if (!manualClose) open() }, delay)
}

function startWatchdog() {
  clearInterval(watchdogTimer)
  watchdogTimer = setInterval(() => {
    // 服务端每 30s 一次 ping，超过 75s 无任何消息视为死链，强制重连
    if (Date.now() - lastMsgAt > 75000) {
      teardown()
      store.wsStatus = 'reconnecting'
      scheduleReconnect()
    }
  }, 20000)
}
