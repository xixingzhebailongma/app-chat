<template>
  <view>
    <view v-if="visible" class="sheet-mask" @tap="onClose"></view>
    <view class="sheet" :class="{ show: visible }">
      <view class="sheet-head">
        <view class="sheet-head-main">
          <text class="sheet-title">{{ title }}</text>
          <text v-if="sub" class="sheet-sub">{{ sub }}</text>
        </view>
        <view class="sheet-close" @tap="onClose">
          <uni-icons type="close" size="18" color="var(--text-4)" />
        </view>
      </view>
      <slot></slot>
    </view>
  </view>
</template>

<script>
export default {
  name: 'ActionSheet',
  props: {
    visible: { type: Boolean, default: false },
    title: { type: String, default: '' },
    sub: { type: String, default: '' },
  },
  emits: ['close'],
  methods: {
    onClose() {
      this.$emit('close')
    },
  },
}
</script>

<style scoped>
/* 底部弹层骨架：遮罩 + 胶囊面板 + 标题栏 + 关闭，内容由 slot 注入 */
.sheet-mask {
  position: fixed;
  left: 0;
  right: 0;
  top: 0;
  bottom: 0;
  /* 低于 uni-app picker 弹层(999)，否则弹层内嵌的选择器会被 sheet 遮挡 */
  z-index: 990;
  background: rgba(0, 0, 0, 0.5);
}
.sheet {
  position: fixed;
  left: 0;
  right: 0;
  bottom: 0;
  z-index: 991;
  background: #fff;
  border-radius: 16rpx 16rpx 0 0;
  padding: 32rpx 32rpx calc(32rpx + env(safe-area-inset-bottom));
  transform: translateY(100%);
  transition: transform 0.25s ease;
}
.sheet.show {
  transform: translateY(0);
}
.sheet-head {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 16rpx;
}
.sheet-head-main {
  flex: 1;
  min-width: 0;
  display: flex;
  flex-direction: column;
  gap: 6rpx;
}
.sheet-title {
  font-size: 34rpx;
  font-weight: 600;
  color: var(--text-1);
}
.sheet-sub {
  font-size: 22rpx;
  color: var(--text-3);
}
.sheet-close {
  display: flex;
  align-items: center;
  padding: 8rpx;
  flex-shrink: 0;
}
</style>