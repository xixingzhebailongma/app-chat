<template>
  <!-- 环境卡网格骨架：2 列，形状对齐 env-card -->
  <view v-if="type === 'env'" class="sk-env">
    <view v-for="n in count" :key="'e' + n" class="sk-env__item">
      <view class="sk-env__head">
        <view class="sk sk-env__icon" />
        <view class="sk sk-env__label" />
      </view>
      <view class="sk sk-env__num" />
    </view>
  </view>

  <!-- 设备列表骨架：行高 120rpx，图标/两行文案/开关位置对齐 device-row -->
  <view v-else-if="type === 'device'">
    <view v-for="n in count" :key="'d' + n" class="sk-dev">
      <view class="sk sk-dev__icon" />
      <view class="sk-dev__lines">
        <view class="sk sk-dev__name" />
        <view class="sk sk-dev__sub" />
      </view>
      <view class="sk sk-dev__sw" />
    </view>
  </view>

  <!-- 告警列表骨架：图标+标题+标签 / 两行简述 / 时间 -->
  <view v-else>
    <view v-for="n in count" :key="'a' + n" class="sk-ac">
      <view class="sk-ac__top">
        <view class="sk sk-ac__icon" />
        <view class="sk sk-ac__title" />
        <view class="sk sk-ac__tag" />
      </view>
      <view class="sk sk-ac__line1" />
      <view class="sk sk-ac__line2" />
      <view class="sk sk-ac__time" />
    </view>
  </view>
</template>

<script setup>
/**
 * 骨架屏（卡片形状/数量与真实布局一致，禁止全屏转圈遮住页面框架）
 * @prop type  env | device | alarm
 * @prop count 卡片数量
 */
defineProps({
  type: { type: String, default: 'device' },
  count: { type: Number, default: 3 },
})
</script>

<style lang="scss" scoped>
/* 灰块 + 扫光高光（灰阶取 card-2 / 高光白） */
.sk {
  position: relative;
  overflow: hidden;
  background: $disabled-light;
  border-radius: 8rpx;
}
.sk::after {
  content: '';
  position: absolute;
  top: 0;
  bottom: 0;
  left: 0;
  width: 40%;
  background: $skel-highlight;
  animation: sc-scan 1.2s linear infinite;
}

/* 环境卡 */
.sk-env {
  display: flex;
  flex-wrap: wrap;
  justify-content: space-between;
  padding: 0 $page-pad;
}
.sk-env__item {
  width: 334rpx;
  background: $bg-card;
  border-radius: $r-card;
  border: 1rpx solid $line;
  box-shadow: $shadow-card;
  padding: $sp-24;
  margin-bottom: $sp-20;
}
.sk-env__head { @include flex(flex-start, center); gap: $sp-12; margin-bottom: $sp-20; }
.sk-env__icon { width: 64rpx; height: 64rpx; border-radius: $r-icon; }
.sk-env__label { width: 130rpx; height: 26rpx; }
.sk-env__num { width: 180rpx; height: 52rpx; border-radius: 8rpx; }

/* 设备行 */
.sk-dev {
  @include flex(flex-start, center);
  gap: $sp-20;
  min-height: 120rpx;
  background: $bg-card;
  border-radius: $r-card;
  border: 1rpx solid $line;
  box-shadow: $shadow-card;
  padding: $sp-20 $sp-24;
  margin: 0 $page-pad $sp-20;
}
.sk-dev__icon { width: 72rpx; height: 72rpx; border-radius: $r-icon; flex: none; }
.sk-dev__lines { flex: 1; @include flex(center, flex-start, column); gap: 14rpx; }
.sk-dev__name { width: 55%; height: 26rpx; }
.sk-dev__sub { width: 38%; height: 22rpx; }
.sk-dev__sw { width: 92rpx; height: 52rpx; border-radius: 26rpx; flex: none; }

/* 告警卡 */
.sk-ac {
  background: $bg-card;
  border-radius: $r-card;
  border: 1rpx solid $line;
  box-shadow: $shadow-card;
  padding: $sp-24;
  margin: 0 $page-pad $sp-20;
  overflow: hidden;
}
.sk-ac__top { @include flex(flex-start, center); gap: $sp-12; margin-bottom: $sp-20; }
.sk-ac__icon { width: 40rpx; height: 40rpx; border-radius: 50%; flex: none; }
.sk-ac__title { flex: 1; height: 28rpx; }
.sk-ac__tag { width: 96rpx; height: 36rpx; border-radius: $r-tag; flex: none; }
.sk-ac__line1 { width: 92%; height: 24rpx; margin-bottom: 12rpx; }
.sk-ac__line2 { width: 70%; height: 24rpx; margin-bottom: $sp-20; }
.sk-ac__time { width: 30%; height: 22rpx; }
</style>
