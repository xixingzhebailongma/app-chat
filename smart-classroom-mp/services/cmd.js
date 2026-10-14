/**
 * 设备控制「三段式确认」
 *
 * ① POST /cmd 拿到 200 → 仅代表指令已下发 MQTT，UI 进入 pending（加载态），不更新开关
 *    HTTP 400/404/500 → 回滚并提示具体原因
 * ② 等 WebSocket cmd_response（全局广播！必须按 device_id 匹配本机指令）
 *    status='ok' → 指令被设备接收；status='error' → 回滚 + 提示
 * ③ 等 WebSocket device_update（按 space 订阅时服务端已过滤）→ 同步最终状态，解除 pending
 *
 * 兜底：超时（CMD_TIMEOUT）未完成 → 回滚 UI，提示「执行结果未知，请稍后刷新查看」
 */
import { sendDeviceCmd } from '../api'
import { CMD_TIMEOUT } from '../api/config'
import { store, mergeDeviceUpdate } from '../store'
import { onWs } from './ws'

let inited = false

export function initCmdService() {
  if (inited) return
  inited = true

  // ③ device_update：合并最终状态并解除 pending（无论 cmd_response 是否已到）
  onWs('device_update', (patch) => {
    mergeDeviceUpdate(patch)
    if (patch.device_id && store.pendingCmds[patch.device_id]) {
      delete store.pendingCmds[patch.device_id]
    }
  })

  // ② cmd_response：全局广播，按 device_id（+command）匹配本机指令
  onWs('cmd_response', (payload) => {
    const p = store.pendingCmds[payload.device_id]
    if (!p || p.command !== payload.command) return // 非本机发出的指令
    if (payload.status === 'ok') {
      p.phase = 'confirmed' // 已被设备接收，等待 device_update 同步最终状态
    } else {
      delete store.pendingCmds[payload.device_id]
      uni.showToast({ title: '设备执行失败：' + (payload.reason || '指令执行出错'), icon: 'none' })
      uni.$emit('cmd:failed', payload)
    }
  })
}

/**
 * 执行设备指令（页面统一入口）
 * @param {Object} device 设备对象（含 device_type / device_id）
 * @param {'on'|'off'|'toggle'} command
 */
export async function execDeviceCommand(device, command) {
  const id = device.device_id
  if (store.pendingCmds[id]) {
    return { error: { message: '该设备有指令正在执行，请稍候' } }
  }

  store.pendingCmds[id] = { command, at: Date.now(), phase: 'sent' }

  // ① 下发指令
  const { error } = await sendDeviceCmd(device.device_type, id, command)
  if (error) {
    delete store.pendingCmds[id]
    return { error } // HTTP 失败 → 回滚（页面 toast 具体原因）
  }

  // 兜底：超时未确认 → 回滚 UI
  setTimeout(() => {
    const p = store.pendingCmds[id]
    if (!p) return
    delete store.pendingCmds[id]
    if (p.phase !== 'confirmed') {
      uni.showToast({ title: '执行结果未知，请稍后刷新查看', icon: 'none' })
    }
    uni.$emit('devices:refresh')
  }, CMD_TIMEOUT)

  return { data: true }
}
