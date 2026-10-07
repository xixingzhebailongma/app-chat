<template>
  <view class="tabbar">
    <view
      v-for="item in tabs"
      :key="item.key"
      class="tab"
      :class="{ active: current === item.key }"
      @tap="go(item)"
    >
      <view class="tic">
        <uni-icons :type="item.icon" size="24" color="currentColor" />
        <view v-if="item.key === 'alerts' && badge > 0" class="badge">{{ badge > 99 ? '99+' : badge }}</view>
      </view>
      <text class="label">{{ item.label }}</text>
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
    tabs() {
      if (store.role === 'teacher') {
        return [
          { key: 'classroom', label: '教室', icon: TAB_ICONS.classroom },
          { key: 'devices', label: '设备', icon: TAB_ICONS.devices },
          { key: 'alerts', label: '告警', icon: TAB_ICONS.alerts },
          { key: 'profile', label: '我的', icon: TAB_ICONS.profile },
        ]
      }
      return [
        { key: 'home', label: '首页', icon: TAB_ICONS.home },
        { key: 'devices', label: '设备', icon: TAB_ICONS.devices },
        { key: 'alerts', label: '告警', icon: TAB_ICONS.alerts },
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
.tabbar {
  position: fixed;
  left: 0;
  right: 0;
  bottom: 0;
  z-index: 999;
  display: flex;
  height: 110rpx;
  padding-bottom: env(safe-area-inset-bottom);
  background: #ffffff;
  border-top: 1rpx solid var(--border-light);
  box-shadow: 0 -4rpx 16rpx rgba(0, 0, 0, 0.04);
}
.tab {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  color: var(--text-3);
}
.tab.active {
  color: var(--primary);
}
.tab .label {
  margin-top: 6rpx;
  font-size: 20rpx;
  line-height: 1;
}
.tic {
  position: relative;
  width: 48rpx;
  height: 48rpx;
  display: flex;
  align-items: center;
  justify-content: center;
}
.badge {
  position: absolute;
  top: -6rpx;
  right: -12rpx;
  min-width: 30rpx;
  height: 30rpx;
  padding: 0 8rpx;
  box-sizing: border-box;
  background: var(--border-lighter);
  color: var(--text-2);
  font-size: 20rpx;
  line-height: 30rpx;
  text-align: center;
  border-radius: 8rpx;
  border: 2rpx solid #fff;
}
</style>
