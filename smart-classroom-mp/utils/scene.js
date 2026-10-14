/**
 * 场景快捷控制：上课模式 / 离开模式
 * 二次确认文案明确说明将影响哪些设备。
 */
import { isSwitchDevice, isOffline } from '../utils/device'
import { execDeviceCommand } from '../services/cmd'
import { COLORS } from '../config/theme'

export const SCENES = {
  class: {
    key: 'class',
    name: '上课模式',
    desc: '开启照明、唤醒综合屏、放下投影幕布',
    command: 'on',
    pick: (ds) => ds.filter((d) => isSwitchDevice(d) && /灯|照明|幕|屏/.test((d.label || '') + d.device_type)),
  },
  leave: {
    key: 'leave',
    name: '离开模式',
    desc: '关闭全部开关设备，门锁联动布防',
    command: 'off',
    pick: (ds) => ds.filter((d) => isSwitchDevice(d)),
  },
}

export function confirmModal(title, content, confirmText = '确认执行') {
  return new Promise((resolve) => {
    uni.showModal({
      title,
      content,
      confirmText,
      confirmColor: COLORS.danger,
      success: (res) => resolve({ confirm: !!res.confirm }),
      fail: () => resolve({ confirm: false }),
    })
  })
}

export async function runScene(key, devices) {
  const scene = SCENES[key]
  if (!scene) return
  const targets = scene.pick(devices || []).filter((d) => !isOffline(d))
  if (!targets.length) {
    uni.showToast({ title: '当前没有可控的在线设备', icon: 'none' })
    return
  }
  const action = scene.command === 'on' ? '开启' : '关闭'
  const names = targets.slice(0, 6).map((t) => t.label || t.device_id).join('、') + (targets.length > 6 ? ` 等 ${targets.length} 台设备` : '')
  const { confirm } = await confirmModal(scene.name, `将${action}：${names}。确认执行？`)
  if (!confirm) return
  // 依次下发（错峰 150ms，避免 MQTT 瞬时拥塞），每台设备独立走三段式确认
  for (const d of targets) {
    execDeviceCommand(d, scene.command)
    await new Promise((r) => setTimeout(r, 150))
  }
  uni.showToast({ title: `「${scene.name}」已下发`, icon: 'none' })
}
