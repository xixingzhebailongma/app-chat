// 边侧 /ws 代理连接：设备状态实时同步（device_update）+ 控制回执（cmd_response）。
// 网关 WebSocket 代理见 wechat-gateway/src/controllers/src/WsProxyController.cpp，
// 契约：ws://<gateway>/ws?token=<jwt>&space_id=<sid>，回推 device_update/cmd_response/ping。
import config from '../config'
import { getToken } from './auth'

let socketTask = null

function wsBase() {
  if (config.WS_BASE_URL) return config.WS_BASE_URL
  return (config.BASE_URL || '').replace(/^http/, 'ws')
}

// 连接网关 /ws 代理。mock 模式下不连，直接返回 null。
// onMessage(data)：收到一条已解析（JSON）的消息；onClose()：连接断开/出错。
export function connectWs(spaceId, { onMessage, onClose } = {}) {
  closeWs()
  if (config.USE_MOCK) return null
  const token = getToken()
  if (!token) return null

  const qs =
    'token=' + encodeURIComponent(token) +
    (spaceId ? '&space_id=' + encodeURIComponent(spaceId) : '')
  const url = wsBase() + '/ws?' + qs

  const task = uni.connectSocket({ url, complete: () => {} })
  socketTask = task
  task.onMessage((res) => {
    let data = res.data
    if (typeof data === 'string') {
      try {
        data = JSON.parse(data)
      } catch (e) {
        // 非 JSON 原样透传
      }
    }
    if (onMessage) onMessage(data)
  })
  task.onClose(() => {
    socketTask = null
    if (onClose) onClose()
  })
  task.onError(() => {
    socketTask = null
    if (onClose) onClose()
  })
  return task
}

export function closeWs() {
  if (socketTask) {
    try {
      socketTask.close({})
    } catch (e) {}
    socketTask = null
  }
}
