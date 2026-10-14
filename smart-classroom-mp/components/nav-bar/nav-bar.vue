<template>
  <!-- fixed=false 时不渲染占位（用于头区自绘的页面，配合 overlay=true） -->
  <view v-if="renderHolder" :style="{ height: metrics.totalH + 'px' }" />

  <view
    class="nav-bar"
    :class="['nav-bar--' + theme, { 'nav-bar--overlay': overlay }]"
    :style="barStyle"
  >
    <view :style="{ height: metrics.statusH + 'px' }" />
    <view class="nav-bar__body" :style="{ height: metrics.contentH + 'px' }">
      <!-- 返回：热区 ≥ 88rpx，用透明 padding 撑开 -->
      <view
        v-if="back"
        class="nav-bar__back"
        hover-class="nav-bar__press"
        :hover-stay-time="80"
        @tap="onBack"
      >
        <app-icon name="back" :size="40" :color="iconColor" />
      </view>

      <!-- 标题：absolute 左右等距，以胶囊为基准居中 -->
      <text
        class="nav-bar__title"
        :style="{ left: metrics.rightInset + 'px', right: metrics.rightInset + 'px' }"
      >{{ title }}</text>

      <view class="nav-bar__right">
        <slot name="right" />
      </view>
    </view>
  </view>
</template>

<script setup>
/**
 * 自定义导航栏
 * @prop theme   gradient(首页品牌蓝渐变) | white(非首页白底黑标题)
 * @prop back    是否显示返回箭头
 * @prop fixed   是否占位（页面内容自动下推）
 * @prop overlay 透明悬浮（首页头区自绘渐变时使用，不占位、无底色）
 */
import { computed } from 'vue'
import { getNavBarMetrics } from '../../utils/layout.js'
import { COLORS } from '../../config/theme'

const props = defineProps({
  title: { type: String, default: '' },
  theme: { type: String, default: 'gradient' }, // gradient | white
  back: { type: Boolean, default: false },
  fixed: { type: Boolean, default: true },
  overlay: { type: Boolean, default: false },
})
const emit = defineEmits(['back'])

const metrics = getNavBarMetrics()
const renderHolder = computed(() => props.fixed && !props.overlay)

const barStyle = computed(() => ({
  paddingTop: '0px',
  background: props.overlay
    ? 'transparent'
    : props.theme === 'gradient' ? COLORS.primary : COLORS.card,
}))

const iconColor = computed(() =>
  props.overlay || props.theme === 'gradient' ? COLORS.white : COLORS.text1
)

function onBack() {
  emit('back')
  const pages = getCurrentPages()
  if (pages.length > 1) {
    uni.navigateBack({ delta: 1 })
  }
}
</script>

<style lang="scss" scoped>
.nav-bar {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  z-index: 900;
}
.nav-bar--white {
  border-bottom: 1rpx solid $line;
}
.nav-bar__body {
  position: relative;
  @include flex(center, center);
}
.nav-bar__title {
  position: absolute;
  top: 0;
  bottom: 0;
  @include flex(center, center);
  text-align: center;
  font-size: $fs-nav-title;
  font-weight: 600;
  pointer-events: none;
  overflow: hidden;
  white-space: nowrap;
  text-overflow: ellipsis;
}
.nav-bar--gradient .nav-bar__title { color: $bg-card; }
.nav-bar--white .nav-bar__title { color: $text-1; }
.nav-bar--white { background: $bg-card; }
.nav-bar--overlay .nav-bar__title { color: $bg-card; }

.nav-bar__back {
  position: absolute;
  left: $sp-12;
  top: 50%;
  transform: translateY(-50%);
  width: 88rpx;
  height: 88rpx;
  @include flex(center, center);
  border-radius: 50%;
  z-index: 2;
}
.nav-bar__press { opacity: 0.6; }

.nav-bar__right {
  position: absolute;
  right: $sp-24;
  top: 0;
  bottom: 0;
  @include flex(center, center);
  gap: $sp-12;
  z-index: 2;
}
</style>
