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
/* ============================================================
 * 全局设计系统 · Design Tokens
 * 风格来源：qingzhou.html「晴昼」主题（暖米底 + 暖白渐变面板 +
 *           青墨主色 #245e70 + 低饱和柔和投影）
 * 规则：变量名保持不变、只统一取值，页面无需改动即自动继承。
 *       兼容旧别名（--ink/--brand/--primary/--bg 等）一律保留。
 * 单位换算：设计稿 1px = 2rpx（750rpx 基准）
 * 对比度：正文 ≥4.5:1、大字/图标 ≥3:1 已实测（详见 docs 计划 §4）
 * ============================================================ */
page {
  /* ---------- 色彩 · 表面 ---------- */
  --bg-page: #f4efe5;    /* 页面背景：晴昼暖米（qingzhou --bg） */
  --bg-card: #fffdf8;    /* 卡片/容器背景：暖白（--panel） */
  --bg-card-end: #f5f1e8;/* 面板渐变末端（--panel-end） */
  --bg-soft: #f1eadb;    /* 输入框/次级填充（--room） */
  --bg-mask: rgba(36, 59, 73, 0.45);
  --bg: var(--bg-page); /* 兼容旧别名（页面在用） */

  /* ---------- 色彩 · 文字 ---------- */
  --text-1: #243b49;    /* 主文本（--text）暖白底 11.50:1 */
  --text-2: #4d6571;    /* 次级文本（--muted）6.05:1 */
  --text-3: #526b77;    /* 辅助/副标题（--faint）5.54:1 */
  --text-4: #9aa9ac;    /* 占位/禁用（2.39:1，仅图标，禁止承载正文） */

  /* ---------- 色彩 · 描边 ---------- */
  --border: #becdcf;         /* 装饰分隔（1.43:1，不可作交互边界） */
  --border-light: #d5dbd4;   /* 卡片发丝线 */
  --border-lighter: #e8e2d4;
  --line-strong: #75969a;    /* 交互边界：输入框/开关轮廓（暖白底 3.14:1） */

  /* ---------- 主按钮：皇家蓝 ---------- */
  --ink: #245e70;        /* 青墨主色（--accent）暖白底 7.10:1 */
  --ink-2: #1b4a59;
  --brand: #245e70;
  --brand-deep: #1b4a59;
  --brand-grad: linear-gradient(135deg, #2F7085 0%, #245E70 100%);
  --brand-soft: #e1ece5; /* = --selected-bg */

  /* ---------- 品牌交互色（选中态/链接/开关 ON） ---------- */
  --primary: #245e70;
  --primary-hover: #2f7085;
  --primary-active: #1b4a59;
  --primary-soft: rgba(36, 94, 112, 0.10);
  --primary-border: rgba(36, 94, 112, 0.24);
  --selected-bg: #e1ece5;   /* 选中底（--selected-bg） */
  --ambient-a: #eadfc8;     /* 环境光 A：页面顶部暖晕（--ambient-a） */
  --ambient-b: #dce9e5;     /* 环境光 B：右下青晕（--ambient-b） */

  /* ---------- 语义色（状态标签；保色相，按暖白底压明度到 AA+） ---------- */
  --success: #0F7038;      /* 面板 6.08:1 / 页面 5.40:1（旧值仅 2.18:1，不可用） */
  --success-bg: #E6F0E8;
  --success-border: #B3CFB8;
  --warning: #A34A08;      /* 面板 5.83:1 / 页面 5.18:1（旧值仅 2.16:1，不可用） */
  --warning-bg: #F6E8CE;
  --warning-border: #E0C08C;
  --danger: #C62828;       /* 面板 5.53:1 / 页面 4.91:1（旧值仅 3.49:1，未达 AA） */
  --danger-bg: #F7E4E1;
  --danger-border: #E3B2AD;
  --info: #526b77;         /* = --text-3；面板 5.54:1 */
  --info-bg: #f1eadb;      /* = --bg-soft */
  --info-border: #d5dbd4;  /* = --border-light */

  /* ---------- 数据可视化色阶（统一青墨系，避免与语义色冲突） ---------- */
  --acc-blue: #245e70;
  --acc-blue-light: #4a8fa0;
  --acc-blue-soft: #9ec6c9;
  --acc-blue-pale: #e1ece5;
  --acc-rainbow: linear-gradient(90deg, #e1ece5 0%, #9ec6c9 35%, #4a8fa0 70%, #245e70 100%);

  /* ---------- 仪表与分区（主页面视觉升级 D 系列） ---------- */
  --gauge-track: #d7dfda;   /* 弧线/进度轨道 */
  --gauge-start: #477d8d;   /* 弧线起点 */
  --gauge-end: #245e70;     /* 弧线终点 */
  --panel-hi: rgba(255, 255, 255, 0.7);    /* 卡片顶部内高光 */
  --zone-face: rgba(255, 253, 248, 0.72);  /* 空间概览分区面 */

  /* ---------- 圆角 ---------- */
  --radius-card: 40rpx;   /* 卡片 20px */
  --radius-btn: 32rpx;    /* 按钮 16px（主按钮可用全胶囊 --radius-pill） */
  --radius-item: 24rpx;   /* 列表项/输入框 12px */
  --radius-pill: 999rpx;  /* 胶囊 */
  --radius-full: 50%;     /* 圆形图标 */
  --radius: 24rpx;        /* 兼容旧名 */
  --radius-lg: var(--radius-card);   /* 兼容旧名（卡片） */
  --radius-xl: 48rpx;     /* 兼容旧名（弹窗/大容器） */

  /* ---------- 阴影（柔和，禁止硬阴影/粗边框） ---------- */
  --shadow-soft: 0 14rpx 36rpx rgba(96, 81, 51, 0.075);   /* 卡片（qingzhou --shadow） */
  --shadow-float: 0 12rpx 32rpx rgba(96, 81, 51, 0.10);   /* 悬浮面板/底栏 */
  /* qingzhou .panel：外投影 + 顶部 1rpx 内高光（暖底上靠高光分层） */
  --shadow-panel: 0 44rpx 110rpx rgba(96, 81, 51, 0.075), inset 0 1rpx 0 rgba(255, 255, 255, 0.07);
  --shadow-btn: 0 12rpx 24rpx rgba(36, 94, 112, 0.26);    /* 主按钮：青墨投影 */
  --shadow: var(--shadow-soft);       /* 兼容旧名 */
  --shadow-card: var(--shadow-soft);  /* 兼容旧名 */

  /* ---------- 间距 ---------- */
  --pad-page: 32rpx;   /* 页面左右边距 16px */
  --pad-card: 40rpx;   /* 卡片内边距 20px */
  --gap-card: 28rpx;   /* 卡片间距 */

  /* ---------- 字号 ---------- */
  --fs-h1: 48rpx;    /* 大标题 24px 加粗 */
  --fs-h2: 36rpx;    /* 卡片标题 18px 加粗 */
  --fs-sub: 28rpx;   /* 副标题 14px 灰 */
  --fs-num: 64rpx;   /* 数据大数字 */

  background: var(--bg-page);
  background-attachment: fixed;
  color: var(--text-1);
  font-size: 30rpx;
  line-height: 1.5;
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto,
    "Helvetica Neue", "PingFang SC", "Hiragino Sans GB",
    "Microsoft YaHei", sans-serif;
  -webkit-font-smoothing: antialiased;
}

view, text, button, input, scroll-view {
  box-sizing: border-box;
}

/* #ifdef H5 */
/* H5 下 body 底色与 page 一致，避免内容不足一屏时底部露出异色 */
body {
  background: #f4efe5;
}
/* uni-app H5 基础样式带 html,body{user-select:none}，会让页面内所有文本
   （包括 textarea 已输入的内容）都无法用鼠标划选复制。
   这里按「输入框可编辑 + 正文可选中」放开，保持像桌面浏览器一样的选中复制体验。 */
html, body {
  -webkit-user-select: text;
  -moz-user-select: text;
  user-select: text;
}
/* #endif */

/* 通用卡片：暖白渐变面板 + 大圆角 + 顶部内高光 + 柔和外投影
   （对齐 qingzhou .panel：靠「渐变 + 高光 + 投影」分层，不用粗边框） */
.card {
  position: relative;
  background: linear-gradient(140deg, var(--bg-card) 0%, var(--bg-card-end) 100%);
  border-radius: var(--radius-card);
  box-shadow: var(--shadow-panel);
}
/* 卡片顶部内高光：1rpx 渐变发丝线，给平面卡一点厚度 */
.card::before {
  content: '';
  position: absolute;
  top: 0;
  left: 24rpx;
  right: 24rpx;
  height: 1rpx;
  pointer-events: none;
  background: linear-gradient(90deg, transparent, var(--panel-hi), transparent);
}

/* 分组列表：纯白块内多行，行间 1rpx 发丝线 */
.list-group {
  background: var(--bg-card);
  border-radius: var(--radius-card);
  overflow: hidden;
  box-shadow: var(--shadow-soft);
}
.list-cell {
  display: flex;
  align-items: flex-start;
  min-height: 112rpx;
  padding: 28rpx 32rpx;
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
  background: var(--bg-soft);
}
.cell-arrow {
  width: 14rpx;
  height: 14rpx;
  border-top: 2rpx solid var(--text-4);
  border-right: 2rpx solid var(--text-4);
  transform: rotate(45deg);
  margin-left: 12rpx;
  flex-shrink: 0;
}

/* section 标题（灰色小节标题，更大留白） */
.section-title {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin: 40rpx 8rpx 20rpx;
}
.section-title .t {
  font-size: 30rpx;
  font-weight: 600;
  color: var(--text-2);
}
.section-title .more {
  font-size: 26rpx;
  color: var(--text-3);
}
.section-title .more:active {
  color: var(--primary);
}

/* 通用按钮：全胶囊，高 96rpx，纯黑实底 + 柔和投影（对齐参考图 Log in / Turn On） */
.btn {
  display: flex;
  align-items: center;
  justify-content: center;
  height: 96rpx;
  border-radius: var(--radius-pill);
  font-size: 30rpx;
  font-weight: 600;
  border: none;
  transition: transform 0.15s, opacity 0.15s;
}
.btn::after {
  border: none;
}
.btn:active {
  transform: scale(0.97);
}
.btn-primary {
  background: var(--ink);
  color: #ffffff;
  box-shadow: var(--shadow-btn);
}
.btn-primary:active {
  background: var(--ink-2);
}
.btn-primary[disabled] {
  background: var(--text-4);
  color: #ffffff;
  box-shadow: none;
}
.btn-ghost {
  background: var(--bg-soft);
  color: var(--text-2);
}
.btn-ghost:active {
  background: var(--border);
}
.btn-plain {
  background: var(--bg-card);
  color: var(--text-2);
  border: 1rpx solid var(--border) !important;
}

/* ============================================================
 * 参考图核心组件（按需取用，不改动任何页面逻辑）
 * ============================================================ */

/* 顶部导航：左对齐大标题 + 右侧圆形图标按钮（浅灰底） */
.nav-head {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 24rpx var(--pad-page) 8rpx;
}
.nav-head-title {
  font-size: var(--fs-h1);
  font-weight: 700;
  color: var(--text-1);
  letter-spacing: 1rpx;
}
.icon-btn {
  width: 80rpx;
  height: 80rpx;
  display: flex;
  align-items: center;
  justify-content: center;
  border-radius: var(--radius-full);
  background: var(--bg-soft);
  flex-shrink: 0;
}
.icon-btn--dark {
  background: var(--ink);
}
.icon-btn:active {
  opacity: 0.7;
}

/* 数据卡：白色大圆角卡 + 标签 + 大号数字 */
.data-card {
  background: linear-gradient(140deg, var(--bg-card) 0%, var(--bg-card-end) 100%);
  border-radius: var(--radius-card);
  box-shadow: var(--shadow-panel);
  padding: var(--pad-card);
}
.data-label {
  font-size: var(--fs-sub);
  color: var(--text-3);
}
.data-num {
  font-family: "DIN Alternate", "Helvetica Neue", Arial, sans-serif;
  font-size: var(--fs-num);
  font-weight: 700;
  line-height: 1.1;
  color: var(--text-1);
  font-variant-numeric: tabular-nums;
}

/* 卡片标题 / 副标题字号分级 */
.h-card {
  font-size: var(--fs-h2);
  font-weight: 600;
  color: var(--text-1);
}
.sub-text {
  font-size: var(--fs-sub);
  color: var(--text-3);
}

/* 开关（Toggle）：青墨轨道 + 暖白圆点胶囊
   关闭态轨道用 --line-strong（暖白底 3.14:1），保证边界可辨（R5） */
.toggle {
  position: relative;
  width: 96rpx;
  height: 56rpx;
  border-radius: var(--radius-pill);
  background: var(--line-strong);
  transition: background 0.24s;
  flex-shrink: 0;
}
.toggle::after {
  content: '';
  position: absolute;
  top: 4rpx;
  left: 4rpx;
  width: 48rpx;
  height: 48rpx;
  border-radius: 50%;
  background: var(--bg-card);
  box-shadow: 0 2rpx 6rpx rgba(36, 59, 73, 0.18);
  transition: transform 0.24s cubic-bezier(0.25, 0.1, 0.25, 1);
}
.toggle--on {
  background: var(--ink);
}
.toggle--on::after {
  transform: translateX(40rpx);
}

/* 悬浮式底部导航：白色圆角面板 + 柔和阴影 */
.dock {
  position: fixed;
  left: var(--pad-page);
  right: var(--pad-page);
  bottom: 0;
  z-index: 800;
  display: flex;
  align-items: center;
  padding: 14rpx 20rpx;
  background: rgba(255, 253, 248, 0.94);
  border-radius: var(--radius-card);
  box-shadow: var(--shadow-float);
  padding-bottom: calc(14rpx + constant(safe-area-inset-bottom));
  padding-bottom: calc(14rpx + env(safe-area-inset-bottom));
}
@supports ((-webkit-backdrop-filter: blur(1px)) or (backdrop-filter: blur(1px))) {
  .dock {
    backdrop-filter: blur(20rpx);
    -webkit-backdrop-filter: blur(20rpx);
  }
}

/* 小标签胶囊 */
.tag {
  display: inline-flex;
  align-items: center;
  padding: 0 16rpx;
  border-radius: var(--radius-pill);
  font-size: 24rpx;
  line-height: 36rpx;
  white-space: nowrap;
}
.tag--red    { background: var(--danger-bg);  color: var(--danger); }
.tag--orange { background: var(--warning-bg); color: var(--warning); }
.tag--green  { background: var(--success-bg); color: var(--success); }
.tag--gray   { background: var(--info-bg);    color: var(--text-2); }
.tag--blue   { background: var(--primary-soft); color: var(--primary); }

/* 等宽数字工具类 */
.num {
  font-family: "DIN Alternate", "Helvetica Neue", Arial, sans-serif;
  font-variant-numeric: tabular-nums;
}

/* 省略 */
.ellipsis {
  overflow: hidden;
  white-space: nowrap;
  text-overflow: ellipsis;
}
.ellipsis-2 {
  display: -webkit-box;
  -webkit-box-orient: vertical;
  -webkit-line-clamp: 2;
  overflow: hidden;
}

/* 按压反馈 */
.press { opacity: 0.7; }
.press-scale { transform: scale(0.97); }

/* 页面底部安全区留白（给自定义 tabbar 让位） */
.page-pad {
  /* 与 smart-classroom-mp 的 .safe-bottom（240rpx）对齐，保证内容不被固定 TabBar 遮挡 */
  padding-bottom: calc(240rpx + env(safe-area-inset-bottom));
}

/* ============================================================
 * 晴昼 1.4.2 追加（对齐 qingzhou.html）
 * 以下三类是纯展示工具类，不绑定任何业务状态
 * ============================================================ */

/* 页面环境光：qingzhou body 的双 radial 叠层。
   用法：加在页面根 view 上（.cv / .dv / .hm / .pf …）。
   注意：page 只保留纯色兜底，避免 fixed 背景在小程序里被裁切。 */
.page-glow {
  background:
    radial-gradient(ellipse at 30% -10%, var(--ambient-a), transparent 55%),
    radial-gradient(ellipse at 95% 70%, var(--ambient-b), transparent 52%),
    var(--bg-page);
}

/* 大写眉标：面板标题上方的小字标（qingzhou .caps） */
.caps {
  font-size: 22rpx;
  line-height: 1.4;
  letter-spacing: 3rpx;
  text-transform: uppercase;
  font-weight: 500;
  color: var(--text-3);
}

/* 四态语义图例（qingzhou .semantic-legend）
   色值走语义 token，未参与本次配色收敛，后续调 D2 时自动跟随 */
.legend {
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: 12rpx 24rpx;
  font-size: 22rpx;
  line-height: 1.4;
  color: var(--text-3);
}
.legend-item {
  display: inline-flex;
  align-items: center;
  gap: 8rpx;
}
.legend-dot {
  width: 12rpx;
  height: 12rpx;
  border-radius: 50%;
  background: var(--text-4);
  flex-shrink: 0;
}
.legend-dot--online { background: var(--success); }
.legend-dot--offline { background: var(--info); }
.legend-dot--alarm { background: var(--danger); }
.legend-dot--unknown { background: var(--warning); }
</style>
