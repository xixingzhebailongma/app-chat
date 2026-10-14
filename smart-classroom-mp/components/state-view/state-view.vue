<template>
  <!-- 五态视图：无权限 / 加载骨架 / 错误 / 空 / 正常内容（离线态由 offline-banner 承担） -->
  <view>
    <!-- 无权限态 -->
    <view v-if="perm === false" class="sv">
      <view class="sv__icon sc-float">
        <app-icon name="lock" :size="72" :color="COLORS.text3" />
      </view>
      <text class="sv__title">暂无访问权限</text>
      <text class="sv__desc">{{ permText || '当前角色无权访问此页面' }}</text>
      <button class="c-btn c-btn--ghost sv__btn" hover-class="press" @tap="$emit('perm-action')">{{ permAction || '返回' }}</button>
    </view>

    <!-- 加载态：骨架屏（不全屏转圈遮内容） -->
    <view v-else-if="loading" class="sv-sk">
      <view v-for="i in rows" :key="i" class="sv-sk__row">
        <view class="sv-sk__avatar sv-sk--anim" />
        <view class="sv-sk__lines">
          <view class="sv-sk__line sv-sk__line--40 sv-sk--anim" />
          <view class="sv-sk__line sv-sk__line--80 sv-sk--anim" />
        </view>
      </view>
    </view>

    <!-- 错误态：可读原因 + 重试按钮 -->
    <view v-else-if="error" class="sv">
      <view class="sv__icon sc-float">
        <app-icon name="warning" :size="72" :color="COLORS.danger" />
      </view>
      <text class="sv__title">加载失败</text>
      <text class="sv__desc">{{ error }}</text>
      <button class="c-btn c-btn--primary sv__btn" hover-class="press" @tap="$emit('retry')">重新加载</button>
    </view>

    <!-- 空态：引导文案 + 行动按钮 -->
    <view v-else-if="empty" class="sv">
      <view class="sv__icon sc-float">
        <app-icon :name="emptyIcon || 'info'" :size="72" :color="COLORS.text4" />
      </view>
      <text class="sv__title">{{ emptyText || '暂无数据' }}</text>
      <text v-if="emptyDesc" class="sv__desc">{{ emptyDesc }}</text>
      <button v-if="emptyAction" class="c-btn c-btn--ghost sv__btn" hover-class="press" @tap="$emit('empty-action')">{{ emptyAction }}</button>
    </view>

    <!-- 正常内容 -->
    <slot v-else />
  </view>
</template>

<script setup>
/**
 * 五态视图容器（加载 / 空 / 错误 / 无权限 + 内容插槽）
 * 离线态不在此组件内：由 offline-banner 横幅承担并保留旧数据
 */
import { COLORS } from '../../config/theme'

defineProps({
  loading: { type: Boolean, default: false },
  error: { type: String, default: '' },
  empty: { type: Boolean, default: false },
  emptyText: { type: String, default: '' },
  emptyDesc: { type: String, default: '' },
  emptyAction: { type: String, default: '' },
  emptyIcon: { type: String, default: '' },   // app-icon 图标名
  perm: { type: Boolean, default: null },     // 传 false 显示无权限态
  permText: { type: String, default: '' },
  permAction: { type: String, default: '返回' },
  rows: { type: Number, default: 4 },         // 骨架屏行数
})
defineEmits(['retry', 'empty-action', 'perm-action'])
</script>

<style lang="scss" scoped>
.sv {
  @include flex(center, center, column);
  padding: 120rpx $page-pad 80rpx;
}
.sv__icon {
  width: 120rpx;
  height: 120rpx;
  border-radius: $r-card;
  background: $bg-page;
  @include flex(center, center);
}
.sv__title {
  font-size: $fs-card-title;
  font-weight: 600;
  color: $text-1;
  margin-top: $sp-24;
}
.sv__desc {
  font-size: $fs-aux;
  color: $text-3;
  margin-top: $sp-12;
  text-align: center;
  line-height: 40rpx;
}
.sv__btn {
  margin-top: $sp-40;
  width: 320rpx;
}

/* 骨架屏 */
.sv-sk { padding: $sp-24 $page-pad; }
.sv-sk__row {
  @include flex(flex-start, center);
  padding: $sp-20 0;
}
.sv-sk__avatar {
  width: 84rpx;
  height: 84rpx;
  border-radius: $r-icon;
  margin-right: $sp-24;
  flex-shrink: 0;
}
.sv-sk__lines { flex: 1; }
.sv-sk__line {
  height: 24rpx;
  border-radius: 12rpx;
  margin: 12rpx 0;
}
.sv-sk__line--40 { width: 40%; }
.sv-sk__line--80 { width: 80%; }
.sv-sk--anim {
  background: $disabled-light;
  position: relative;
  overflow: hidden;
}
.sv-sk--anim::after {
  content: '';
  position: absolute;
  inset: 0;
  width: 40%;
  background: $skel-highlight;
  animation: sc-scan 1.2s ease infinite;
}
</style>
