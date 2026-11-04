<template>
  <view class="tabbar-wrap">
    <view class="tabbar">
      <view
        v-for="item in tabs"
        :key="item.key"
        class="tab"
        :class="{ active: current === item.key }"
        @tap="go(item)"
      >
        <view class="tic">
          <uni-icons :type="current === item.key ? item.icon + '-filled' : item.icon" size="22" color="currentColor" />
          <view v-if="item.key === 'alerts' && badge > 0" class="badge">{{ badge > 99 ? '99+' : badge }}</view>
        </view>
        <text class="label">{{ item.label }}</text>
      </view>
    </view>
  </view>
</template>

<script>
import { store } from '../store/index'
import { TAB_ICONS } from '../utils/icon'

export default {
  name: 'AppTabBar',
  props: {
    current: { type: String, default: 'home' },
  },
  computed: {
    badge() {
      return store.unhandledAlerts
    },
    // 底部 tab 按角色区分：教师首 tab 是「我的教室」，管理员是「首页」。
    // 文案与 smart-classroom-mp 教师端统一（我的教室 / 设备控制 / 告警 / AI / 我的）。
    tabs() {
      if (store.role === 'teacher') {
        return [
          { key: 'classroom', label: '我的教室', icon: TAB_ICONS.classroom },
          { key: 'devices', label: '设备控制', icon: TAB_ICONS.devices },
          { key: 'alerts', label: '告警', icon: TAB_ICONS.alerts },
          { key: 'ai', label: 'AI', icon: TAB_ICONS.ai },
          { key: 'profile', label: '我的', icon: TAB_ICONS.profile },
        ]
      }
      return [
        { key: 'home', label: '首页', icon: TAB_ICONS.home },
        { key: 'devices', label: '设备', icon: TAB_ICONS.devices },
        { key: 'alerts', label: '告警', icon: TAB_ICONS.alerts },
        { key: 'ai', label: 'AI', icon: TAB_ICONS.ai },
        { key: 'profile', label: '我的', icon: TAB_ICONS.profile },
      ]
    },
  },
  methods: {
    go(item) {
      if (item.key === this.current) return
      uni.reLaunch({ url: '/pages/' + item.key + '/' + item.key })
    },
  },
}
</script>

<style scoped>
/* 悬浮圆角 dock：左右留 24rpx、底部让开安全区，不再被 home indicator 压住文字 */
.tabbar-wrap {
  position: fixed;
  left: 24rpx;
  right: 24rpx;
  bottom: calc(16rpx + constant(safe-area-inset-bottom));
  bottom: calc(16rpx + env(safe-area-inset-bottom));
  z-index: 800;
  border-radius: 32rpx;
  border: 1rpx solid var(--border-light);
  background: rgba(255, 253, 248, 0.95);
  box-shadow: 0 18rpx 44rpx rgba(96, 81, 51, 0.16);
  overflow: hidden;
}
@supports ((-webkit-backdrop-filter: blur(1px)) or (backdrop-filter: blur(1px))) {
  .tabbar-wrap {
    background: rgba(255, 253, 248, 0.88);
    backdrop-filter: blur(24px);
    -webkit-backdrop-filter: blur(24px);
  }
}
.tabbar {
  display: flex;
  flex-direction: row;
  justify-content: space-around;
  align-items: stretch;
  height: 104rpx;
}
.tab {
  flex: 1;
  display: flex;
  flex-direction: column;
  justify-content: center;
  align-items: center;
  gap: 4rpx;
  color: var(--text-3);
  transition: opacity 150ms cubic-bezier(0.25, 0.1, 0.25, 1);
}
.tab:active {
  opacity: 0.7;
}
.tab.active {
  color: var(--primary);
}
/* 图标盒固定 48rpx 高，图标在其中水平垂直居中，保证四个 tab 图标齐平 */
.tic {
  position: relative;
  height: 48rpx;
  display: flex;
  align-items: center;
  justify-content: center;
  transition: transform 200ms cubic-bezier(0.25, 0.1, 0.25, 1);
}
.tab.active .tic {
  transform: translateY(-4rpx);
}
.tab.active .tic::after {
  content: '';
  position: absolute;
  bottom: -8rpx;
  left: 50%;
  transform: translateX(-50%);
  width: 8rpx;
  height: 8rpx;
  border-radius: 50%;
  background: var(--primary);
}
.tab .label {
  font-size: 24rpx;
  line-height: 1;
  transition: color 200ms cubic-bezier(0.25, 0.1, 0.25, 1), font-weight 200ms cubic-bezier(0.25, 0.1, 0.25, 1);
}
.tab.active .label {
  font-weight: 600;
}
.badge {
  position: absolute;
  top: -14rpx;
  left: 30rpx;
  min-width: 30rpx;
  height: 30rpx;
  padding: 0 8rpx;
  box-sizing: border-box;
  background: var(--danger);
  color: #ffffff;
  font-size: 18rpx;
  line-height: 30rpx;
  text-align: center;
  border-radius: 16rpx;
  box-shadow: 0 2rpx 6rpx rgba(198, 40, 40, 0.3);
}
</style>
