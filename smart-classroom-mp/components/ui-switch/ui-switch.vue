<template>
  <view
    class="sw"
    :class="{ 'sw--on': value, 'sw--pending': pending, 'sw--disabled': disabled }"
    @tap.stop="onTap"
  >
    <view class="sw__knob" :class="{ 'sw__knob--on': value }">
      <!-- pending：滑块位置不动，白色转圈，整体降透明度 -->
      <view v-if="pending" class="sw__loading sc-spin" />
    </view>
  </view>
</template>

<script setup>
/**
 * 设备控制开关（对齐参考项目 iOS 风胶囊 46×26px → 92×52rpx）
 * 视觉状态机：关(card-2 灰底+细边) / 开(品牌蓝) / pending(不动+转圈) / 离线(.5不可点)
 * 只负责展示与派发；三段式确认由 services/cmd.js + 页面编排。
 *
 * @prop value    是否开启
 * @prop pending  指令执行中（不下发新指令）
 * @prop disabled 设备离线不可控
 */
const props = defineProps({
  value: { type: Boolean, default: false },
  pending: { type: Boolean, default: false },
  disabled: { type: Boolean, default: false },
})
const emit = defineEmits(['switch'])

function onTap() {
  if (props.disabled || props.pending) return
  emit('switch', !props.value)
}
</script>

<style lang="scss" scoped>
.sw {
  position: relative;
  width: 92rpx;
  height: 52rpx;
  border-radius: 26rpx;
  background: $disabled-light;
  border: 1rpx solid $line;
  transition: background-color $t-switch $ease-standard,
              opacity $t-switch $ease-standard,
              border-color $t-switch $ease-standard;
  flex: none;
}
.sw--on {
  background: $primary;
  border-color: $primary;
}
/* pending：立刻进入品牌蓝“已下发”视觉，滑块不动 + 白色转圈 */
.sw--pending {
  opacity: 0.85;
  background: $primary;
  border-color: $primary;
}
.sw--disabled { opacity: 0.5; }

.sw__knob {
  position: absolute;
  top: 5rpx;
  left: 5rpx;
  width: 40rpx;
  height: 40rpx;
  border-radius: 50%;
  background: #fff;
  box-shadow: 0 2rpx 6rpx rgba(0, 0, 0, 0.15);
  /* 只用 transform 滑动，200ms */
  transform: translateX(0);
  transition: transform $t-switch $ease-standard;
  @include flex(center, center);
}
.sw__knob--on { transform: translateX(40rpx); }

.sw__loading {
  width: 26rpx;
  height: 26rpx;
  border-radius: 50%;
  border: 3rpx solid rgba(255, 255, 255, 0.65);
  border-top-color: #fff;
}
</style>
