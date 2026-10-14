/**
 * API 模块：全部业务请求收敛于此。
 * 每个方法与真实接口的映射关系见注释；USE_MOCK=true 时走演示数据。
 * 业务代码禁止在页面里硬编码任何路径。
 */
import { PATHS, USE_MOCK } from './config'
import { request } from './request'
import * as mock from './mock'

function qs(params = {}) {
  const s = Object.keys(params)
    .filter((k) => params[k] !== '' && params[k] !== undefined && params[k] !== null)
    .map((k) => `${k}=${encodeURIComponent(params[k])}`)
    .join('&')
  return s ? '?' + s : ''
}

/* ---------- 鉴权 ---------- */
/** POST /api/auth/login → { token, user: { user_id, username, name, role } } */
export function login(username, password) {
  if (USE_MOCK) return mock.login(username, password)
  return request({ url: PATHS.login, method: 'POST', data: { username, password }, auth: false })
}

/* ---------- 设备 ---------- */
/** GET /api/devices?space_id=xxx */
export function getDevices(spaceId) {
  if (USE_MOCK) return mock.getDevices(spaceId)
  return request({ url: PATHS.devices + qs({ space_id: spaceId }) })
}

/** POST /api/devices/{device_type}/{device_id}/cmd  body: { command } */
export function sendDeviceCmd(deviceType, deviceId, command) {
  if (USE_MOCK) return mock.sendDeviceCmd(deviceType, deviceId, command)
  return request({ url: PATHS.deviceCmd(deviceType, deviceId), method: 'POST', data: { command } })
}

/* ---------- 今日进出记录 ---------- */
/** GET /api/face/sessions（★ 未在现有文档定义，待后端确认路径与参数） */
export function getFaceSessions(params = {}) {
  if (USE_MOCK) return mock.getFaceSessions(params)
  return request({ url: PATHS.faceSessions + qs(params) })
}

/* ---------- 课表 ---------- */
/** GET /api/board/schedules?space_id=xxx */
export function getSchedules(spaceId) {
  if (USE_MOCK) return mock.getSchedules(spaceId)
  return request({ url: PATHS.schedules + qs({ space_id: spaceId }) })
}

/* ---------- 教室目录 / SIP 在线状态（免认证） ---------- */
/** GET /provision/sip/endpoints?refresh=1 */
export function getSipEndpoints(refresh) {
  if (USE_MOCK) return mock.getSipEndpoints(refresh)
  return request({ url: PATHS.sipEndpoints + (refresh ? '?refresh=1' : ''), auth: false })
}

/* ---------- 管理员：全局总览（云端聚合接口，路径待定） ---------- */
export function getOverview() {
  if (USE_MOCK) return mock.getOverview()
  return request({ url: PATHS.overview })
}

/* ---------- 告警（云端接口，路径待定） ---------- */
export function getAlarms(params = {}) {
  if (USE_MOCK) return mock.getAlarms(params)
  return request({ url: PATHS.alarmList + qs(params) })
}

export function getAlarmDetail(id) {
  if (USE_MOCK) return mock.getAlarmDetail(id)
  return request({ url: PATHS.alarmDetail(id) })
}

export function handleAlarm(id, payload) {
  if (USE_MOCK) return mock.handleAlarm(id, payload)
  return request({ url: PATHS.alarmHandle(id), method: 'POST', data: payload })
}

/* ---------- 用户与教室绑定管理（云端接口，路径待定） ---------- */
export function getUsers() {
  if (USE_MOCK) return mock.getUsers()
  return request({ url: PATHS.users })
}

export function bindUserSpace(userId, spaceId) {
  if (USE_MOCK) return mock.bindUserSpace(userId, spaceId)
  return request({ url: PATHS.userBind(userId), method: 'POST', data: { space_id: spaceId } })
}

export function unbindUserSpace(userId, spaceId) {
  if (USE_MOCK) return mock.unbindUserSpace(userId, spaceId)
  return request({ url: PATHS.userBind(userId), method: 'DELETE', data: { space_id: spaceId } })
}

/* ---------- 高风险操作（云端接口，路径待定） ---------- */
export function rebootDevice(deviceId) {
  if (USE_MOCK) return mock.rebootDevice(deviceId)
  return request({ url: PATHS.deviceCmd('fuhe-screen', deviceId), method: 'POST', data: { command: 'reboot' } })
}
