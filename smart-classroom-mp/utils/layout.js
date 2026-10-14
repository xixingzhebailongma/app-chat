/**
 * 自定义导航栏几何尺寸：
 * - statusBarHeight：状态栏（动态适配刘海/灵动岛）
 * - contentH：标题区高度。微信端按右上角胶囊按钮反推，保证垂直对齐；
 * - rightInset：胶囊左缘到屏幕右边的距离，标题据此居中（以胶囊为基准）。
 */
let cached = null

export function getNavBarMetrics() {
  if (cached) return cached

  // #ifdef MP-WEIXIN
  const sys = uni.getSystemInfoSync()
  const statusH = sys.statusBarHeight || 20
  let contentH = 44
  let rightInset = 0
  let capsuleW = 0
  try {
    const menu = uni.getMenuButtonBoundingClientRect && uni.getMenuButtonBoundingClientRect()
    if (menu && menu.height) {
      // 胶囊上下边距相等 → 标题区高度 = 间距×2 + 胶囊高
      contentH = (menu.top - statusH) * 2 + menu.height
      rightInset = sys.windowWidth - menu.left
      capsuleW = menu.width
    }
  } catch (e) { /* 低版本取不到时使用默认值 */ }
  cached = {
    statusH,
    contentH,
    rightInset,
    capsuleW,
    totalH: statusH + contentH,
  }
  return cached
  // #endif

  // #ifdef H5
  cached = { statusH: 0, contentH: 44, rightInset: 0, capsuleW: 0, totalH: 44 }
  return cached
  // #endif

  // #ifndef MP-WEIXIN || H5
  cached = { statusH: 20, contentH: 44, rightInset: 0, capsuleW: 0, totalH: 64 }
  return cached
  // #endif
}
