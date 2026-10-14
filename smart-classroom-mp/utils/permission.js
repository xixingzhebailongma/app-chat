/**
 * 角色权限统一判断：页面里禁止散落 if (role === 'xxx')
 * 唯一数据源 PERM_MAP；角色 → 首页 / tabBar 配置也收敛在此。
 */
import { store } from '../store'

export const ROLE = { TEACHER: 'teacher', ADMIN: 'admin' }

export function isAdmin(u = store.user) { return !!u && u.role === 'admin' }
export function isTeacher(u = store.user) { return !!u && u.role === 'teacher' }

const PERM_MAP = {
  'device:control': ['teacher', 'admin'],
  'scene:control': ['teacher', 'admin'],
  'alarm:view': ['teacher', 'admin'],
  'alarm:handle': ['admin'],
  'alarm:history': ['admin'],
  'device:remote': ['admin'],
  'user:manage': ['admin'],
  'space:all': ['admin'],
}

export function can(perm, u = store.user) {
  const roles = PERM_MAP[perm] || []
  return !!u && roles.includes(u.role)
}

export const SHELL_PATH = '/pages/shell/shell'

export function roleHome(u = store.user) {
  return SHELL_PATH
}

/**
 * 跨页跳转到某个 tab（壳层内 v-show 视图，切换不重载）。
 * - 当前就在壳层页：直接派发事件切换
 * - 在子页面（告警详情/教室详情等，均由壳层 navigateTo 打开）：先派发再返回
 */
export function gotoTab(key, extra = {}) {
  uni.$emit('shell:goto', Object.assign({ key }, extra))
  const pages = getCurrentPages()
  const cur = pages[pages.length - 1]
  const onShell = cur && String(cur.route || '').indexOf('pages/shell/shell') > -1
  if (!onShell) {
    uni.navigateBack({ fail: () => uni.redirectTo({ url: SHELL_PATH }) })
  }
}

/**
 * 角色 → tabBar 项（自定义 TabBar 按角色渲染）
 * key：稳定逻辑键（角标/壳层缓存按 key 匹配，不随文案/路径变化）
 * icon/iconOn：app-icon 图标名（线性 / 填充）
 * 注意：所有 tab 均为壳层内视图，path 仅供语义参考，不再做页面跳转
 */
export function roleTabs(u = store.user) {
  if (isAdmin(u)) {
    return [
      { key: 'overview', path: SHELL_PATH, text: '总览', icon: 'grid',    iconOn: 'grid-fill' },
      { key: 'alarm',    path: SHELL_PATH, text: '告警', icon: 'bell',    iconOn: 'bell-fill' },
      { key: 'control',  path: SHELL_PATH, text: '控制', icon: 'control', iconOn: 'control-fill' },
      { key: 'mine',     path: SHELL_PATH, text: '我的', icon: 'user',    iconOn: 'user-fill' },
    ]
  }
  return [
    { key: 'classroom', path: SHELL_PATH, text: '我的教室', icon: 'home',    iconOn: 'home-fill' },
    { key: 'control',   path: SHELL_PATH, text: '设备控制', icon: 'control', iconOn: 'control-fill' },
    { key: 'alarm',     path: SHELL_PATH, text: '告警',     icon: 'bell',    iconOn: 'bell-fill' },
    { key: 'mine',      path: SHELL_PATH, text: '我的',     icon: 'user',    iconOn: 'user-fill' },
  ]
}

/** 登录态守卫：未登录直接踢回登录页 */
export function requireAuth() {
  if (!store.token) {
    uni.reLaunch({ url: '/pages/login/login' })
    return false
  }
  return true
}
