/**
 * 全局响应式 Store（免依赖 Pinia）
 * space_id 不在本地硬编码：由云端登录响应返回（教师=绑定教室，管理员=教室目录）
 */
import { reactive } from 'vue'

const AUTH_KEY = 'sc_auth'

export const store = reactive({
  token: '',
  user: null,          // { user_id, username, name, role }
  spaceId: '',         // 教师：绑定教室 space_id；管理员：当前操作教室
  spaces: [],          // 教室目录 [{ space_id, name }]
  devices: [],         // 当前 space 的设备全量快照
  devicesAt: 0,        // 快照时间
  pendingCmds: {},     // 三段式确认：device_id -> { command, at, phase: 'sent'|'confirmed' }
  wsStatus: 'offline', // offline | connecting | online | reconnecting
  netOnline: true,
  tick: 0,             // 相对时间刷新信号
  subscribed: uni.getStorageSync('sc_subscribed') === 'yes',
  pendingAlarmCount: 0, // TabBar 告警角标
})

export function loadAuth() {
  try {
    const a = uni.getStorageSync(AUTH_KEY)
    if (a && a.token) {
      store.token = a.token
      store.user = a.user || null
      store.spaceId = a.spaceId || ''
      store.spaces = a.spaces || []
    }
  } catch (e) { /* 忽略本地缓存异常 */ }
}

export function saveAuth(token, user, extra = {}) {
  store.token = token
  store.user = user
  store.spaceId = extra.space_id || (user && user.space_id) || ''
  store.spaces = extra.spaces || (user && user.spaces) || []
  uni.setStorageSync(AUTH_KEY, {
    token, user,
    spaceId: store.spaceId,
    spaces: store.spaces,
  })
}

export function clearAuth() {
  store.token = ''
  store.user = null
  store.spaceId = ''
  store.spaces = []
  store.devices = []
  store.pendingCmds = {}
  store.pendingAlarmCount = 0
  uni.removeStorageSync(AUTH_KEY)
}

export function setDevices(list) {
  store.devices = list || []
  store.devicesAt = Date.now()
}

/** device_update 增量合并（按 space 订阅时服务端已过滤，可直接使用） */
export function mergeDeviceUpdate(patch) {
  if (!patch || !patch.device_id) return
  const i = store.devices.findIndex((d) => d.device_id === patch.device_id)
  if (i > -1) {
    store.devices[i] = Object.assign({}, store.devices[i], patch)
  } else if (patch.space_id === store.spaceId) {
    store.devices.push(patch)
  }
}

export function setSubscribed(v) {
  store.subscribed = !!v
  uni.setStorageSync('sc_subscribed', v ? 'yes' : 'no')
}

export function spaceName(id) {
  const s = store.spaces.find((x) => x.space_id === id)
  return s ? s.name : ''
}
