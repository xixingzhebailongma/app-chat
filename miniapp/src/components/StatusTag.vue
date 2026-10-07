<template>
  <view class="tag" :class="'tone-' + tone">
    <view v-if="dot" class="dot"></view>
    <text>{{ text }}</text>
  </view>
</template>

<script>
// tone: success | warning | danger | info | neutral（Element Plus 语义色）
export default {
  name: 'StatusTag',
  props: {
    text: { type: String, default: '' },
    tone: { type: String, default: 'neutral' },
    dot: { type: Boolean, default: true },
  },
}
</script>

<style scoped>
.tag {
  display: inline-flex;
  align-items: center;
  height: 40rpx;
  padding: 0 14rpx;
  border-radius: 8rpx;
  font-size: 22rpx;
  line-height: 1;
  color: var(--text-2);
  background: var(--bg-soft);
  border: 1rpx solid var(--border-lighter);
}
.dot {
  width: 12rpx;
  height: 12rpx;
  border-radius: 50%;
  margin-right: 8rpx;
  background: var(--dot-c, currentColor);
}
.tone-success { --dot-c: var(--success); }
.tone-warning { --dot-c: var(--warning); }
.tone-danger { --dot-c: var(--danger); }
.tone-info { --dot-c: var(--primary); }
.tone-neutral { --dot-c: var(--info); }
</style>
