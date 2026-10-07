<script>
import { store, isLoggedIn } from './store/index'
import { refreshTemplateIds } from './utils/subscribe'

// 冷启动标记：onLaunch 后的第一次 onShow 是冷启动，entry 页面会被自动打开，无需手动导航。
let cold = true

function extractAlertId(options) {
  const q = options && options.query
  const path = options && options.path
  if (!q || !q.id) return ''
  if (path && path.indexOf('pages/alerts/detail') === -1) return ''
  return q.id
}

// 未登录时把目标记进 store，登录成功后由 login.vue 消费。
function recordPending(options) {
  const id = extractAlertId(options)
  if (id && !isLoggedIn()) store.pendingAlertId = id
}

function currentRoute() {
  const pages = getCurrentPages()
  const top = pages[pages.length - 1]
  if (!top) return ''
  return top.route || (top.$page && top.$page.route) || ''
}

function goToAlert(id) {
  if (!isLoggedIn()) {
    store.pendingAlertId = id
    uni.reLaunch({ url: '/pages/login/login' })
    return
  }
  const url = '/pages/alerts/detail?id=' + encodeURIComponent(id)
  // 已停在 detail 页：redirectTo 原地替换，避免 navigateTo 叠加页面栈。
  if (currentRoute() === 'pages/alerts/detail') {
    uni.redirectTo({ url })
  } else {
    uni.navigateTo({ url })
  }
}

function navigateDeepLink(options) {
  const id = extractAlertId(options)
  if (id) goToAlert(id)
}

export default {
  onLaunch(options) {
    cold = true
    recordPending(options)
    // 会话持久化（已登录）场景：启动即拉一次订阅模板 ID（优先接口，失败降级 env）
    if (isLoggedIn()) refreshTemplateIds()
  },
  onShow(options) {
    if (cold) {
      cold = false
      recordPending(options)   // 冷启动：entry 页面自动打开，只兜底「未登录」记目标
      return
    }
    navigateDeepLink(options)   // 从后台回前台 / 点订阅消息唤醒：手动导航
  },
  onHide() {},
}
</script>

<style>
/* ============ 全局设计系统（微信原生风：分组列表 + 灰色小节标题 + 去彩色强调） ============ */
page {
  --primary: #409eff;
  --primary-hover: #79bbff;
  --primary-active: #337ecc;
  --primary-soft: #ecf5ff;
  --primary-border: #d9ecff;

  --text-1: #303133;
  --text-2: #606266;
  --text-3: #909399;
  --text-4: #c0c4cc;

  --border: #dcdfe6;
  --border-light: #e4e7ed;
  --border-lighter: #ebeef5;
  --bg: #f5f7fa;
  --bg-soft: #f5f7fa;

  --success: #67c23a;
  --success-bg: #f0f9eb;
  --success-border: #e1f3d8;
  --warning: #e6a23c;
  --warning-bg: #fdf6ec;
  --warning-border: #faecd8;
  --danger: #f56c6c;
  --danger-bg: #fef0f0;
  --danger-border: #fde2e2;
  --info: #909399;
  --info-bg: #f4f4f5;
  --info-border: #e9e9eb;

  --radius: 8rpx;
  --radius-lg: 12rpx;
  --shadow: 0 1rpx 4rpx rgba(0, 0, 0, 0.04);

  background: var(--bg);
  color: var(--text-1);
  font-size: 28rpx;
  line-height: 1.5;
  font-family: -apple-system, BlinkMacSystemFont, "Helvetica Neue", "PingFang SC",
    "Hiragino Sans GB", "Microsoft YaHei", sans-serif;
}

/* 通用卡片（白底 + 细边框 + 小圆角，去阴影更贴近原生） */
.card {
  background: #ffffff;
  border: 1rpx solid var(--border-light);
  border-radius: var(--radius-lg);
}

/* 微信式分组列表：一个大白块内多行，行间 1rpx 发丝线 */
.list-group {
  background: #ffffff;
  border-radius: var(--radius-lg);
  overflow: hidden;
}
.list-cell {
  display: flex;
  align-items: flex-start;
  min-height: 96rpx;
  padding: 24rpx 24rpx;
  box-sizing: border-box;
  border-bottom: 1rpx solid var(--border-lighter);
}
.list-cell:last-child {
  border-bottom: none;
}
.list-cell-center {
  align-items: center;
}
.cell-hover {
  background: #f2f3f5;
}
.cell-arrow {
  width: 16rpx;
  height: 16rpx;
  border-top: 2rpx solid var(--text-4);
  border-right: 2rpx solid var(--text-4);
  transform: rotate(45deg);
  margin-left: 12rpx;
  flex-shrink: 0;
}

/* section 标题（微信风灰色小节标题） */
.section-title {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin: 32rpx 4rpx 16rpx;
}
.section-title .t {
  font-size: 28rpx;
  font-weight: 500;
  color: var(--text-3);
}
.section-title .more {
  font-size: 24rpx;
  color: var(--text-3);
}
.section-title .more:active {
  color: var(--primary);
}

/* 通用按钮（Element Plus button） */
.btn {
  display: flex;
  align-items: center;
  justify-content: center;
  height: 80rpx;
  border-radius: 8rpx;
  font-size: 28rpx;
  font-weight: 400;
  border: 1rpx solid transparent;
  transition: background 0.2s;
}
.btn::after {
  border: none;
}
.btn-primary {
  background: var(--primary);
  border-color: var(--primary);
  color: #ffffff;
}
.btn-primary:active {
  background: var(--primary-active);
}
.btn-ghost {
  background: var(--primary-soft);
  border-color: var(--primary-soft);
  color: var(--primary);
}
.btn-plain {
  background: #ffffff;
  color: var(--text-2);
  border-color: var(--border) !important;
}

/* 页面底部安全区留白（给自定义 tabbar 让位） */
.page-pad {
  padding-bottom: calc(140rpx + env(safe-area-inset-bottom));
}
</style>
