<template>
  <view class="tabbar-wrap">
    <view class="tabbar">
      <view
        v-for="(t, i) in tabs"
        :key="role + ':' + t.key"
        class="tabbar__item"
        hover-class="tabbar__item--press"
        :hover-stay-time="60"
        @tap="onTap(i, t)"
      >
        <view class="tabbar__iconbox">
          <view class="tabbar__icon" :class="{ 'is-on': i === currentIndex }">
            <app-icon
              :name="i === currentIndex ? t.iconOn : t.icon"
              :size="48"
              :color="i === currentIndex ? COLORS.primary : COLORS.text3"
            />
          </view>
          <!-- 未读角标：为 0 不占位；>99 显示 99+ -->
          <view v-if="badge[t.key]" class="tabbar__badge num">
            {{ badge[t.key] > 99 ? '99+' : badge[t.key] }}
          </view>
        </view>
        <text class="tabbar__label" :class="{ 'is-on': i === currentIndex }">{{ t.text }}</text>
      </view>
    </view>
  </view>
</template>

<script setup>
/**
 * 自定义 TabBar（角色动态：教师 3 项 / 管理员 4 项）
 * 仅负责展示与派发事件；页面缓存/切换动画由壳层负责（v-show 方案，不重载）。
 *
 * @prop role    teacher | admin
 * @prop current 当前选中项（支持下标或 key）
 * @prop badge   { alarm: 未读数 } 角标映射
 * @prop mode    emit(默认，只派发事件) | redirect(兼容旧独立页：redirectTo)
 */
import { computed } from 'vue'
import { roleTabs } from '../../utils/permission.js'
import { COLORS } from '../../config/theme'

const props = defineProps({
  role: { type: String, default: 'teacher' },
  current: { type: [Number, String], default: 0 },
  badge: { type: Object, default: () => ({}) },
  mode: { type: String, default: 'emit' },
})
const emit = defineEmits(['change'])

const tabs = computed(() => roleTabs({ role: props.role }))

const currentIndex = computed(() => {
  if (typeof props.current === 'number') return props.current
  const i = tabs.value.findIndex((t) => t.key === props.current)
  return i < 0 ? 0 : i
})

function onTap(i, t) {
  if (i === currentIndex.value) return
  emit('change', { index: i, key: t.key, path: t.path })
  if (props.mode === 'redirect') {
    uni.redirectTo({ url: t.path })
  }
}
</script>

<style lang="scss" scoped>
.tabbar-wrap {
  position: fixed;
  left: 0;
  right: 0;
  bottom: 0;
  z-index: 800;
  background: $bg-card; /* 毛玻璃降级纯色 */
  border-top: 1rpx solid $line;
  @include safe-bottom(0);
}
@supports (backdrop-filter: blur(1px)) {
  .tabbar-wrap {
    background: rgba(255, 255, 255, 0.92);
    backdrop-filter: blur(20px);
    -webkit-backdrop-filter: blur(20px);
  }
}
.tabbar {
  @include flex(space-around, stretch, row);
  height: 100rpx;
}
.tabbar__item {
  flex: 1;
  @include flex(center, center, column);
  gap: 4rpx;
  transition: opacity $t-micro $ease-standard;
}
.tabbar__item--press { opacity: 0.7; }

.tabbar__iconbox {
  position: relative;
  height: 48rpx;
  @include flex(center, center);
}
.tabbar__icon {
  transition: transform $t-switch $ease-standard;
  @include flex(center, center);
}
.tabbar__icon.is-on {
  /* 选中：图标变填充并上浮 4rpx（无下划线） */
  transform: translateY(-4rpx);
}
.tabbar__label {
  font-size: $fs-tag;
  line-height: 1;
  color: $text-3;
  transition: color $t-switch $ease-standard, font-weight $t-switch $ease-standard;
}
.tabbar__label.is-on {
  color: $primary;
  font-weight: 600;
}

.tabbar__badge {
  position: absolute;
  top: -14rpx;
  left: 30rpx;
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
  box-shadow: $shadow-badge;
}
</style>
