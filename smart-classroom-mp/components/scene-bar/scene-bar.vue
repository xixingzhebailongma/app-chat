<template>
  <view class="scb">
    <view
      v-for="s in scenes"
      :key="s.key"
      class="scb__btn"
      :class="'scb__btn--' + s.key"
      hover-class="scb__btn--press"
      :hover-stay-time="60"
      @tap="$emit('scene', s.key)"
    >
      <view class="scb__icon">
        <app-icon :name="s.iconName" :size="44" :color="COLORS.white" />
      </view>
      <view class="scb__text">
        <text class="scb__name">{{ s.name }}</text>
        <text class="scb__desc">{{ s.desc }}</text>
      </view>
    </view>
  </view>
</template>

<script setup>
/**
 * 场景快捷控制按钮组：视觉上比设备列表更突出（渐变底 + 大热区）
 * 点击后由页面调用 runScene 做二次确认（文案说明将影响哪些设备）
 */
import { SCENES } from '../../utils/scene'
import { COLORS } from '../../config/theme'

defineEmits(['scene'])

const scenes = [
  { ...SCENES.class, iconName: 'play' },
  { ...SCENES.leave, iconName: 'power' },
]
</script>

<style lang="scss" scoped>
.scb {
  @include flex(flex-start, stretch);
  gap: $sp-20;
}
.scb__btn {
  flex: 1;
  @include flex(flex-start, center);
  border-radius: $r-card;
  padding: $sp-24 $sp-28;
  min-height: 128rpx;
  box-shadow: $shadow-card;
  transition: transform $t-micro $ease-standard;
}
.scb__btn--press { transform: scale(0.97); }
.scb__btn--class { background: $primary; }
.scb__btn--leave { background: $ink; }
.scb__icon {
  width: 76rpx;
  height: 76rpx;
  border-radius: 50%;
  background: rgba(255, 255, 255, 0.18);
  @include flex(center, center);
  margin-right: $sp-20;
  flex: none;
}
.scb__text { @include flex(flex-start, flex-start, column); min-width: 0; }
.scb__name { color: #fff; font-size: $fs-card-title; font-weight: 700; }
.scb__desc {
  color: rgba(255, 255, 255, 0.75);
  font-size: $fs-tag;
  margin-top: 4rpx;
  @include ellipsis(1);
}
</style>
