<template>
  <view class="scb">
    <view
      v-for="s in scenes"
      :key="s.key"
      class="scb__btn"
      :class="'scb__btn--' + s.key"
      @tap="$emit('scene', s.key)"
    >
      <text class="scb__icon">{{ s.icon }}</text>
      <view class="scb__text">
        <text class="scb__name">{{ s.name }}</text>
        <text class="scb__desc">{{ s.desc }}</text>
      </view>
    </view>
  </view>
</template>

<script setup>
/**
 * 场景快捷控制按钮组：视觉上比设备列表更突出，点击后由页面做二次确认
 */
import { SCENES } from '../utils/scene'

defineEmits(['scene'])

const scenes = Object.keys(SCENES).map((k) => SCENES[k])
</script>

<style lang="scss" scoped>
.scb { display: flex; gap: 20rpx; }
.scb__btn {
  flex: 1;
  display: flex;
  align-items: center;
  border-radius: $radius;
  padding: 26rpx 28rpx;
  box-shadow: $shadow;
  min-height: 120rpx;
}
.scb__btn--class {
  background: linear-gradient(135deg, #6B93FF, $primary);
}
.scb__btn--leave {
  background: linear-gradient(135deg, #334155, #1E293B);
}
.scb__icon { font-size: 48rpx; margin-right: 20rpx; }
.scb__text { display: flex; flex-direction: column; min-width: 0; }
.scb__name { color: #fff; font-size: $fs-lg; font-weight: 700; }
.scb__desc {
  color: rgba(255, 255, 255, 0.75);
  font-size: $fs-xs;
  margin-top: 4rpx;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
</style>
