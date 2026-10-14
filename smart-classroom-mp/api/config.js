/**
 * ===== API 配置（唯一需要按环境修改的文件）=====
 * 本小程序运行在公网，禁止直连边侧内网地址（不得出现 192.168.x.x）。
 * 所有请求经云端后端鉴权/校验教室权限后转发至边侧 go-backend。
 * 云端转发层路径与边侧真实路径可能不同 → 全部收敛在此处，业务代码禁止硬编码路径。
 */

/** 云端网关 HTTP 前缀（部署后替换，例：https://cloud.example.com/gateway） */
export const API_BASE = 'https://cloud.example.com/gateway'

/** 云端 WebSocket 入口（token 通过 URL 传递，WebSocket 不支持自定义 Header） */
export const WS_BASE = 'wss://cloud.example.com/gateway/ws'

/** 请求超时（ms） */
export const REQUEST_TIMEOUT = 10000

/** 设备指令三段式确认兜底超时（ms）：cmd_response 超时未达即回滚 UI */
export const CMD_TIMEOUT = 12000

/** 演示模式：云端接口未就绪时置 true，使用内置 Mock 数据跑通全部界面 */
export const USE_MOCK = true

/** 微信订阅消息模板 ID（微信公众平台申请后替换） */
export const SUBSCRIBE_TMPL_IDS = ['TEMPLATE_ID_PLACEHOLDER']

/**
 * 接口路径表（值为边侧 go-backend 真实路径；云端转发路径不同时只改这里）
 */
export const PATHS = {
  login: '/api/auth/login',
  devices: '/api/devices',
  deviceCmd: (type, id) => `/api/devices/${type}/${id}/cmd`,
  faceSessions: '/api/face/sessions',            // ★ 未在现有文档定义，待后端确认
  schedules: '/api/board/schedules',
  sipEndpoints: '/provision/sip/endpoints',      // 免认证
  ipc: '/provision/ipc',

  // ↓↓↓ 以下为云端侧接口占位，路径待云端确定后替换
  overview: '/api/cloud/overview',
  alarmList: '/api/cloud/alerts',
  alarmDetail: (id) => `/api/cloud/alerts/${id}`,
  alarmHandle: (id) => `/api/cloud/alerts/${id}/handle`,
  alarmHistory: '/api/cloud/alerts/history',
  users: '/api/cloud/users',
  userBind: (uid) => `/api/cloud/users/${uid}/spaces`,
}
