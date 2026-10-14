<script>
import { onLaunch, onShow } from '@dcloudio/uni-app'
import { store, loadAuth } from './store'
import { initCmdService } from './services/cmd'
import { ensureWs } from './services/ws'

export default {
  setup() {
    onLaunch(() => {
      loadAuth()
      // 全局注册一次：cmd_response / device_update 的三段式确认监听
      initCmdService()
      // 相对时间刷新 tick（“x 分钟前”类展示）
      setInterval(() => { store.tick++ }, 30000)
      // 网络状态监听：离线横幅 + 恢复后自动重连 WS
      uni.getNetworkType({ success: r => { store.netOnline = r.networkType !== 'none' } })
      uni.onNetworkStatusChange(res => {
        const wasOffline = !store.netOnline
        store.netOnline = res.isConnected
        if (wasOffline && res.isConnected) ensureWs()
      })
    })

    // 小程序切后台会断开连接：onShow 时重新对齐状态
    onShow(() => { ensureWs() })

    return {}
  }
}
</script>

<style lang="scss">
/* ===== 全局基础样式（所有 Token 取自 uni.scss，禁止硬编码） ===== */
page {
  background: $bg-page;
  font-size: $fs-body;
  color: $text-1;
  font-family: $font-base;
  -webkit-font-smoothing: antialiased;
}

view, text, button, input, scroll-view { box-sizing: border-box; }

/* 等宽数字工具类 */
.num {
  font-family: $font-num;
  font-variant-numeric: tabular-nums;
}

/* 省略 */
.ellipsis { @include ellipsis(1); }
.ellipsis-2 { @include ellipsis(2); }

/* 通用卡片：白卡 + 发丝线分界（Minimalism：靠边界与留白组织，不靠投影） */
.card {
  background: $bg-card;
  border: 1rpx solid $line;
  border-radius: $r-card;
  box-shadow: $shadow-card;
  padding: $card-pad;
  margin: $card-gap $page-pad;
}
.card__title {
  @include flex(space-between, center);
  font-size: $fs-card-title;
  font-weight: 600;
  color: $text-1;
  margin-bottom: $sp-20;
}
.card__more {
  font-size: $fs-aux;
  color: $text-3;
  font-weight: 400;
}

/* 小标签胶囊（全胶囊） */
.tag {
  display: inline-flex;
  align-items: center;
  padding: 0 16rpx;
  border-radius: $r-tag;
  font-size: $fs-tag;
  line-height: 36rpx;
  white-space: nowrap;
}
.tag--red    { background: $danger-light;  color: $danger; }
.tag--orange { background: $warning-light; color: $warning; }
.tag--green  { background: $success-light; color: $success; }
.tag--gray   { background: $disabled-light; color: $text-2; }
.tag--blue   { background: $primary-light; color: $primary; }

/* 按钮：全胶囊，高 96rpx，品牌蓝实底 + 品牌色投影 */
.c-btn {
  border-radius: $r-btn;
  height: 96rpx;
  line-height: 96rpx;
  font-size: 30rpx;
  font-weight: 600;
  text-align: center;
  padding: 0 48rpx;
  margin: 0;
  border: none;
  transition: transform $t-micro $ease-standard, opacity $t-micro $ease-standard;
}
.c-btn::after { border: none; }
/* Minimalism：主按钮投影收敛，强调色本身已足够聚焦 */
.c-btn--primary {
  background: $primary;
  color: #fff;
  box-shadow: 0 2rpx 8rpx rgba(0, 122, 255, 0.18);
}
.c-btn--primary[disabled] { background: $text-4; color: #fff; }
/* 次按钮：card-2 浅灰底 + 次级文字（参考 btn-ghost） */
.c-btn--ghost { background: $disabled-light; color: $text-2; }
/* 危险按钮：浅红底 + 红字（参考 btn-danger）；高风险实底红用 c-btn--danger-solid */
.c-btn--danger { background: $danger-light; color: $danger; }
.c-btn--danger-solid { background: $danger; color: #fff; }
.c-btn--sm {
  height: 64rpx;
  line-height: 64rpx;
  font-size: $fs-aux;
  padding: 0 24rpx;
  border-radius: $r-btn-sm;
}

/* 通用按压反馈（hover-class 引用，热区 ≥ 88rpx 由组件自行保证） */
.press { opacity: 0.7; }
.press-scale { transform: scale(0.97); }

/* 底部占位（兼容旧类名） */
.safe-bottom { height: 240rpx; }

/* 分段选择器 */
.seg {
  display: flex;
  background: $disabled-light;
  border-radius: 44rpx;
  padding: 6rpx;
}
.seg__item {
  flex: 1;
  text-align: center;
  font-size: $fs-aux;
  color: $text-2;
  line-height: 60rpx;
  border-radius: 40rpx;
  transition: all 0.2s;
}
.seg__item--on {
  background: #fff;
  color: $primary;
  font-weight: 600;
  box-shadow: $shadow-seg;
}

/* 列表单元格 */
.cell {
  display: flex;
  align-items: center;
  padding: 26rpx 0;
  border-bottom: 1rpx solid $line;
}
.cell:last-child { border-bottom: none; }
.cell__label { flex: 1; color: $text-2; }
.cell__value { color: $text-1; font-weight: 500; max-width: 60%; text-align: right; }

/* ===== 全局动效关键帧（只允许 transform / opacity） ===== */

/* 在线状态呼吸：opacity .55↔1 + scale 1↔1.15 */
@keyframes sc-breathe {
  0%, 100% { opacity: 1;    transform: scale(1); }
  50%      { opacity: 0.55; transform: scale(1.15); }
}
.sc-breathe { animation: sc-breathe $t-breath $ease-standard infinite; }

/* 转圈（重连中 / pending / 刷新） */
@keyframes sc-spin { to { transform: rotate(360deg); } }
.sc-spin { animation: sc-spin 800ms linear infinite; }

/* 骨架屏扫光：高光从左到右 1.2s 循环 */
@keyframes sc-scan {
  0%   { transform: translateX(-100%); }
  100% { transform: translateX(320%); }
}

/* 空态插画缓慢上下浮动 */
@keyframes sc-float {
  0%, 100% { transform: translateY(0); }
  50%      { transform: translateY(-8rpx); }
}
.sc-float { animation: sc-float 3s $ease-standard infinite; }

/* 内容入场：opacity 0→1 + translateY(16rpx)→0，240ms */
@keyframes sc-fade-up {
  from { opacity: 0; transform: translateY(16rpx); }
  to   { opacity: 1; transform: translateY(0); }
}
.sc-fade-up { animation: sc-fade-up $t-toggle $ease-standard both; }

/* 弹窗遮罩淡入 */
@keyframes sc-mask-in { from { opacity: 0; } to { opacity: 1; } }
/* 弹窗卡片 scale(.92)→1，280ms */
@keyframes sc-pop-in {
  from { opacity: 0; transform: scale(0.92); }
  to   { opacity: 1; transform: scale(1); }
}

/* 连接横幅 slide-down / slide-up */
@keyframes sc-slide-down {
  from { transform: translateY(-100%); }
  to   { transform: translateY(0); }
}
@keyframes sc-slide-up {
  from { transform: translateY(0); }
  to   { transform: translateY(-100%); }
}

/* 输入错误轻抖：左右各 6rpx */
@keyframes sc-shake {
  0%, 100% { transform: translateX(0); }
  20%, 60% { transform: translateX(-6rpx); }
  40%, 80% { transform: translateX(6rpx); }
}
.sc-shake { animation: sc-shake 200ms $ease-standard; }

/* 实时数值高亮闪动：primary 浅底 → 透明，600ms */
@keyframes sc-num-flash {
  0%   { background-color: $primary-light; }
  100% { background-color: transparent; }
}

/* 兼容旧版类名（旧页面逐页重做后可删） */
.dot { width: 14rpx; height: 14rpx; border-radius: 50%; display: inline-block; }
.dot--green  { background: $success; }
.dot--gray   { background: $disabled; }
.dot--orange { background: $warning; }
.dot--red    { background: $danger; }
</style>
