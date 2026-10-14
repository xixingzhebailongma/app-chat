<template>
  <view class="app-icon" :style="boxStyle" />
</template>

<script setup>
/**
 * 单色 SVG 图标（data-URI 内联，跨 H5 / 微信小程序一致）
 * @prop name   图标名，见 icons.js
 * @prop size   图标尺寸，数字按 rpx，也可传 '20px' 字符串
 * @prop color  单色描边/填充色，默认次级文字色
 */
import { computed } from 'vue'
import { ICONS } from './icons.js'

const props = defineProps({
  name: { type: String, required: true },
  size: { type: [Number, String], default: 40 },
  color: { type: String, default: '#3A3A3C' },
})

const boxStyle = computed(() => {
  const inner = (ICONS[props.name] || '')
    .split('{{C}}').join(props.color)
  const svg =
    `<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' ` +
    `stroke='${props.color}' stroke-width='1.8' stroke-linecap='round' ` +
    `stroke-linejoin='round'>${inner}</svg>`
  return {
    width: normalize(props.size),
    height: normalize(props.size),
    // encodeURIComponent 转义 #、引号等，H5 / 小程序背景图均支持
    backgroundImage: `url("data:image/svg+xml,${encodeURIComponent(svg)}")`,
  }
})

function normalize(v) {
  return typeof v === 'number' ? `${v}rpx` : v
}
</script>

<style lang="scss" scoped>
.app-icon {
  display: inline-block;
  flex: none;
  background-repeat: no-repeat;
  background-position: center;
  background-size: contain;
}
</style>
