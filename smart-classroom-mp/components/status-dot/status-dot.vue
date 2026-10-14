<template>
  <view class="sd" :class="'sd--' + status">
    <!-- 在线：绿色呼吸；重连中：橙色慢转圆环；离线：灰色静止 -->
    <view v-if="status === 'reconnecting'" class="sd__ring sc-spin" />
    <view v-else class="sd__point" :class="{ 'sc-breathe': status === 'online' }" />
    <text v-if="showText" class="sd__text">{{ displayText }}</text>
  </view>
</template>

<script setup>
/**
 * 连接/在线状态圆点（14rpx + 24rpx 文字）
 * @prop status online | reconnecting | offline
 */
import { computed } from 'vue'
const props = defineProps({
  status: { type: String, default: 'online' },
  text: { type: String, default: '' },
  showText: { type: Boolean, default: true },
})
const TEXT_MAP = { online: '在线', reconnecting: '重连中', offline: '离线' }
const displayText = computed(() => props.text || TEXT_MAP[props.status] || '')
</script>

<style lang="scss" scoped>
.sd { display: inline-flex; align-items: center; gap: 10rpx; }
.sd__point {
  width: 14rpx;
  height: 14rpx;
  border-radius: 50%;
  flex: none;
}
.sd__ring {
  width: 16rpx;
  height: 16rpx;
  border-radius: 50%;
  border: 3rpx solid $warning;
  border-top-color: transparent;
  flex: none;
}
.sd--online .sd__point { background: $success; }
.sd--reconnecting .sd__text { color: $warning; }
.sd--offline .sd__point { background: $disabled; }
.sd--offline .sd__text { color: $text-3; }
.sd--online .sd__text { color: $success; }
.sd__text { font-size: $fs-aux; line-height: 1; }
</style>
