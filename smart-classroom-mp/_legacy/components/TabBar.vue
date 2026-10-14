<template>
  <view class="tabbar-wrap">
    <view class="tabbar">
      <view
        v-for="t in tabs"
        :key="t.path"
        class="tabbar__item"
        :class="{ 'is-active': t.path === current }"
        @tap="go(t)"
      >
        <view class="tabbar__icon-wrap">
          <text class="tabbar__icon">{{ t.icon }}</text>
          <view v-if="t.badge" class="tabbar__badge">{{ t.badge > 99 ? '99+' : t.badge }}</view>
        </view>
        <text class="tabbar__label">{{ t.text }}</text>
      </view>
    </view>
  </view>
</template>

<script setup>
/**
 * 自定义 TabBar：按角色动态渲染（教师 3 tab / 管理员 4 tab）
 * 均为普通页面 + redirectTo 切换（无原生 tabBar，跨端一致）
 */
import { computed } from 'vue'
import { roleTabs } from '../utils/permission'
import { store } from '../store'

const props = defineProps({
  current: { type: String, default: '' },
})

const tabs = computed(() =>
  roleTabs().map((t) => ({
    ...t,
    badge: t.path === '/pages/alarm/list' ? store.pendingAlarmCount : 0,
  }))
)

function go(t) {
  if (t.path === props.current) return
  uni.redirectTo({ url: t.path })
}
</script>

<style lang="scss" scoped>
.tabbar-wrap {
  position: fixed;
  left: 0;
  right: 0;
  bottom: 0;
  z-index: 998;
  padding-bottom: constant(safe-area-inset-bottom);
  padding-bottom: env(safe-area-inset-bottom);
}
.tabbar {
  display: flex;
  height: 108rpx;
  background: #fff;
  border-top: 1rpx solid $border;
  box-shadow: 0 -4rpx 24rpx rgba(31, 45, 80, 0.06);
}
.tabbar__item {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 2rpx;
  /* 点击热区 ≥ 44px */
  min-height: 100rpx;
}
.tabbar__icon-wrap { position: relative; width: 56rpx; text-align: center; }
.tabbar__icon {
  font-size: 42rpx;
  line-height: 52rpx;
  opacity: 0.45;
  transition: opacity 0.2s, transform 0.2s;
}
.is-active .tabbar__icon { opacity: 1; transform: translateY(-2rpx) scale(1.1); }
.tabbar__label { font-size: $fs-xs; color: $text-3; transition: color 0.2s; }
.is-active .tabbar__label { color: $primary; font-weight: 600; }
.tabbar__badge {
  position: absolute;
  top: -4rpx;
  right: -18rpx;
  min-width: 30rpx;
  height: 30rpx;
  padding: 0 8rpx;
  border-radius: 16rpx;
  background: $danger;
  color: #fff;
  font-size: 18rpx;
  line-height: 30rpx;
  text-align: center;
  box-sizing: border-box;
}
</style>
