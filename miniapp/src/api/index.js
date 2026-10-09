import config from '../config'
import { request } from '../utils/request'
import { mockApi } from './mock'

const base = () => config.BASE_URL
const useMock = () => config.USE_MOCK

// 小程序端 API：mock 与真实网关共用同一套函数签名，切换 config.USE_MOCK 即可。
export const api = {
  // 演示模式：账号密码登录（admin → 管理员，其它 → 教师）
  loginMock(username, password) {
    return mockApi.login(username, password)
  },

  // 真实模式：wx.login code 换 token / need_bind
  wxLogin(code) {
    return request({ url: base() + '/api/miniapp/login', method: 'POST', data: { code } })
  },

  // 真实模式：绑定账号密码 -> 换 JWT
  bind(openidToken, username, password) {
    return request({
      url: base() + '/api/miniapp/bind',
      method: 'POST',
      data: { openid_token: openidToken, username, password },
    })
  },

  me() {
    return useMock() ? mockApi.me() : request({ url: base() + '/api/miniapp/me' })
  },

  spaces() {
    // 真实网关返回 {spaces:[...]} 信封，mock 直接返回数组——统一成数组返回。
    return useMock()
      ? mockApi.spaces()
      : request({ url: base() + '/api/miniapp/spaces' }).then((res) => (res && res.spaces) || [])
  },

  devices(spaceId) {
    const url = spaceId
      ? base() + '/api/miniapp/devices?space_id=' + encodeURIComponent(spaceId)
      : base() + '/api/miniapp/devices'
    return useMock() ? mockApi.devices(spaceId) : request({ url })
  },

  alerts(filters) {
    const parts = []
    if (filters && filters.space_id) parts.push('space_id=' + encodeURIComponent(filters.space_id))
    if (filters && filters.event_type) parts.push('event_type=' + encodeURIComponent(filters.event_type))
    if (filters && filters.status) parts.push('status=' + encodeURIComponent(filters.status))
    if (filters && filters.from) parts.push('from=' + encodeURIComponent(filters.from))
    if (filters && filters.to) parts.push('to=' + encodeURIComponent(filters.to))
    if (filters && filters.page) parts.push('page=' + filters.page)
    if (filters && filters.page_size) parts.push('page_size=' + filters.page_size)
    const qs = parts.length ? '?' + parts.join('&') : ''
    return useMock()
      ? mockApi.alerts(filters)
      : request({ url: base() + '/api/miniapp/alerts' + qs })
  },

  handleAlert(id, remark, status) {
    return useMock()
      ? mockApi.handleAlert(id, remark, status)
      : request({
          url: base() + '/api/miniapp/alerts/' + encodeURIComponent(id) + '/handle',
          method: 'POST',
          data: { remark, status },
        })
  },

  control(payload) {
    return useMock()
      ? mockApi.control(payload)
      : request({ url: base() + '/api/miniapp/device/control', method: 'POST', data: payload })
  },

  // 门禁白名单（读：admin/teacher；写：仅 admin）
  doorDevices(spaceId) {
    const qs = spaceId ? '?space_id=' + encodeURIComponent(spaceId) : ''
    return useMock()
      ? mockApi.doorDevices(spaceId)
      : request({ url: base() + '/api/miniapp/door-devices' + qs })
  },
  markDoor(payload) {
    return useMock()
      ? mockApi.markDoor(payload)
      : request({ url: base() + '/api/miniapp/door-devices', method: 'POST', data: payload })
  },
  unmarkDoor(spaceId, deviceId) {
    const qs = '?space_id=' + encodeURIComponent(spaceId) + '&device_id=' + encodeURIComponent(deviceId)
    return useMock()
      ? mockApi.unmarkDoor(spaceId, deviceId)
      : request({ url: base() + '/api/miniapp/door-devices' + qs, method: 'DELETE' })
  },

  sceneExecute(payload) {
    return useMock()
      ? mockApi.sceneExecute(payload)
      : request({ url: base() + '/api/miniapp/scene/execute', method: 'POST', data: payload })
  },

  subscribe(payload) {
    return useMock()
      ? mockApi.subscribe(payload)
      : request({ url: base() + '/api/miniapp/subscribe', method: 'POST', data: payload })
  },

  getSubscriptions() {
    return useMock()
      ? mockApi.getSubscriptions()
      : request({ url: base() + '/api/miniapp/subscribe', method: 'GET' })
  },

  // 教师-教室绑定（仅 admin）
  spaceBindings() {
    return useMock()
      ? mockApi.spaceBindings()
      : request({ url: base() + '/api/miniapp/space-bindings' })
  },

  bindSpace(userId, spaceId) {
    return useMock()
      ? mockApi.bindSpace(userId, spaceId)
      : request({
          url: base() + '/api/miniapp/space-bindings',
          method: 'POST',
          data: { user_id: userId, space_id: spaceId },
        })
  },

  unbindSpace(userId, spaceId) {
    return useMock()
      ? mockApi.unbindSpace(userId, spaceId)
      : request({
          url:
            base() +
            '/api/miniapp/space-bindings?user_id=' +
            encodeURIComponent(userId) +
            '&space_id=' +
            encodeURIComponent(spaceId),
          method: 'DELETE',
        })
  },

  // 进出记录（空间/日期/方式/分页，服务端做权限过滤）
  accessRecords(spaceId, date, page, pageSize, authType) {
    const parts = []
    if (spaceId) parts.push('space_id=' + encodeURIComponent(spaceId))
    if (date) parts.push('date=' + encodeURIComponent(date))
    if (authType) parts.push('auth_type=' + encodeURIComponent(authType))
    if (page) parts.push('page=' + page)
    if (pageSize) parts.push('page_size=' + pageSize)
    const qs = parts.length ? '?' + parts.join('&') : ''
    return useMock()
      ? mockApi.accessRecords(spaceId, date, page, pageSize, authType)
      : request({ url: base() + '/api/miniapp/access-records' + qs })
  },

  // 运维日志（仅 admin；op_type/space_id/user_id/from/to/分页）
  operationLogs(filters) {
    const f = filters || {}
    const parts = []
    if (f.op_type) parts.push('op_type=' + encodeURIComponent(f.op_type))
    if (f.space_id) parts.push('space_id=' + encodeURIComponent(f.space_id))
    if (f.user_id) parts.push('user_id=' + encodeURIComponent(f.user_id))
    if (f.from) parts.push('from=' + encodeURIComponent(f.from))
    if (f.to) parts.push('to=' + encodeURIComponent(f.to))
    if (f.page) parts.push('page=' + f.page)
    if (f.page_size) parts.push('page_size=' + f.page_size)
    const qs = parts.length ? '?' + parts.join('&') : ''
    return useMock()
      ? mockApi.operationLogs(f)
      : request({ url: base() + '/api/miniapp/operation-logs' + qs })
  },

  // 告警详情时间线（仅 admin；返回 {alert, timeline}）
  alertTimeline(id) {
    return useMock()
      ? mockApi.alertTimeline(id)
      : request({ url: base() + '/api/miniapp/alerts/' + encodeURIComponent(id) + '/timeline' })
  },

  // 单条告警详情（admin/teacher 都可；教师侧 operator 三字段为空串，返回 {alert}）
  alertDetail(id) {
    return useMock()
      ? mockApi.alertDetail(id)
      : request({ url: base() + '/api/miniapp/alerts/' + encodeURIComponent(id) })
  },

  // 订阅消息模板 ID 权威来源（返回 { template_ids: [...] }）
  notifyTemplates() {
    return useMock()
      ? mockApi.notifyTemplates()
      : request({ url: base() + '/api/miniapp/notify/templates' })
  },

  // 告警统计（全量概览，不随列表筛选；admin 全量 / teacher 绑定教室）
  alertStats() {
    return useMock()
      ? mockApi.alertStats()
      : request({ url: base() + '/api/miniapp/alerts/stats' })
  },

  // 空间类型（读：登录可读；写：仅 admin）
  spaceTypes() {
    return useMock()
      ? mockApi.spaceTypes()
      : request({ url: base() + '/api/miniapp/space-types' }).then((res) => (res && res.types) || [])
  },
  createSpaceType(payload) {
    return useMock()
      ? mockApi.createSpaceType(payload)
      : request({ url: base() + '/api/admin/space-types', method: 'POST', data: payload })
  },
  updateSpaceType(code, payload) {
    return useMock()
      ? mockApi.updateSpaceType(code, payload)
      : request({
          url: base() + '/api/admin/space-types/' + encodeURIComponent(code),
          method: 'PUT',
          data: payload,
        })
  },
  disableSpaceType(code) {
    return useMock()
      ? mockApi.disableSpaceType(code)
      : request({
          url: base() + '/api/admin/space-types/' + encodeURIComponent(code),
          method: 'DELETE',
        })
  },
  reorderSpaceTypes(codes) {
    return useMock()
      ? mockApi.reorderSpaceTypes(codes)
      : request({
          url: base() + '/api/admin/space-types/order',
          method: 'PATCH',
          data: { codes },
        })
  },

  // 教室管理（读：admin 列表；写：仅 admin）
  adminSpaces() {
    return useMock()
      ? mockApi.adminSpaces()
      : request({ url: base() + '/api/admin/spaces' }).then((res) => (res && res.spaces) || [])
  },
  createSpace(payload) {
    return useMock()
      ? mockApi.createSpace(payload)
      : request({ url: base() + '/api/admin/spaces', method: 'POST', data: payload })
  },
  updateSpace(spaceId, payload) {
    return useMock()
      ? mockApi.updateSpace(spaceId, payload)
      : request({
          url: base() + '/api/admin/spaces/' + encodeURIComponent(spaceId),
          method: 'PUT',
          data: payload,
        })
  },
  disableSpace(spaceId) {
    return useMock()
      ? mockApi.disableSpace(spaceId)
      : request({
          url: base() + '/api/admin/spaces/' + encodeURIComponent(spaceId),
          method: 'DELETE',
        })
  },
}
