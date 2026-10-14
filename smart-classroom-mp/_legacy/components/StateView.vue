<template>
  <!-- 五态视图：无权限 / 加载 / 错误 / 空 / 正常内容（离线态由 OfflineBanner 承担） -->
  <view>
    <!-- 无权限态 -->
    <view v-if="perm === false" class="state">
      <text class="state__icon">🔒</text>
      <text class="state__title">暂无访问权限</text>
      <text class="state__desc">{{ permText || '当前角色无权访问此页面' }}</text>
      <button class="c-btn c-btn--ghost state__btn" @tap="$emit('perm-action')">{{ permAction || '返回' }}</button>
    </view>

    <!-- 加载态：骨架屏（不全屏转圈遮内容） -->
    <view v-else-if="loading" class="sk">
      <view v-for="i in rows" :key="i" class="sk__row">
        <view class="sk__avatar sk--anim"></view>
        <view class="sk__lines">
          <view class="sk__line sk__line--40 sk--anim"></view>
          <view class="sk__line sk__line--80 sk--anim"></view>
        </view>
      </view>
    </view>

    <!-- 错误态：可读原因 + 重试按钮 -->
    <view v-else-if="error" class="state">
      <text class="state__icon">⚠️</text>
      <text class="state__title">加载失败</text>
      <text class="state__desc">{{ error }}</text>
      <button class="c-btn c-btn--primary state__btn" @tap="$emit('retry')">重新加载</button>
    </view>

    <!-- 空态：引导文案 + 行动按钮 -->
    <view v-else-if="empty" class="state">
      <text class="state__icon">{{ emptyIcon || '📭' }}</text>
      <text class="state__title">{{ emptyText || '暂无数据' }}</text>
      <text v-if="emptyDesc" class="state__desc">{{ emptyDesc }}</text>
      <button v-if="emptyAction" class="c-btn c-btn--ghost state__btn" @tap="$emit('empty-action')">{{ emptyAction }}</button>
    </view>

    <!-- 正常内容 -->
    <slot v-else />
  </view>
</template>

<script setup>
defineProps({
  loading: { type: Boolean, default: false },
  error: { type: String, default: '' },
  empty: { type: Boolean, default: false },
  emptyText: { type: String, default: '' },
  emptyDesc: { type: String, default: '' },
  emptyAction: { type: String, default: '' },
  emptyIcon: { type: String, default: '' },
  perm: { type: Boolean, default: null },   // 传 false 显示无权限态
  permText: { type: String, default: '' },
  permAction: { type: String, default: '返回' },
  rows: { type: Number, default: 4 },       // 骨架屏行数
})
defineEmits(['retry', 'empty-action', 'perm-action'])
</script>

<style lang="scss" scoped>
.state {
  display: flex;
  flex-direction: column;
  align-items: center;
  padding: 120rpx 60rpx 80rpx;
}
.state__icon { font-size: 88rpx; line-height: 120rpx; }
.state__title { font-size: $fs-lg; font-weight: 600; margin-top: 16rpx; }
.state__desc { font-size: $fs-sm; color: $text-3; margin-top: 12rpx; text-align: center; line-height: 40rpx; }
.state__btn { margin-top: 40rpx; width: 320rpx; }

/* 骨架屏 */
.sk { padding: 30rpx; }
.sk__row { display: flex; align-items: center; padding: 20rpx 0; }
.sk__avatar { width: 84rpx; height: 84rpx; border-radius: $radius-sm; margin-right: 24rpx; flex-shrink: 0; }
.sk__lines { flex: 1; }
.sk__line { height: 24rpx; border-radius: 12rpx; margin: 12rpx 0; }
.sk__line--40 { width: 40%; }
.sk__line--80 { width: 80%; }
.sk--anim {
  background: linear-gradient(90deg, #EDF0F5 25%, #F7F9FC 45%, #EDF0F5 65%);
  background-size: 400% 100%;
  animation: skShimmer 1.3s ease infinite;
}
@keyframes skShimmer {
  0% { background-position: 100% 0; }
  100% { background-position: 0 0; }
}
</style>
