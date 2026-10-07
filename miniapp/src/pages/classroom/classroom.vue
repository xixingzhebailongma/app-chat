<template>
  <view class="classroom page-pad">
    <!-- 订阅引导（真实模式，未订阅时显示） -->
    <SubscribePrompt />

    <!-- 空间选择（教师绑定多个教室时出现） -->
    <picker
      v-if="spaces.length > 1"
      mode="selector"
      :range="spaces"
      range-key="name"
      @change="onSpaceChange"
    >
      <view class="space-select card">
        <text class="space-select-label">{{ currentSpace ? currentSpace.name : '选择教室' }}</text>
        <view class="space-select-arrow"><uni-icons type="arrow-down" size="16" color="var(--text-3)" /></view>
      </view>
    </picker>

    <!-- 场景快捷控制 -->
    <view class="section-title"><text class="t">场景快捷控制</text></view>
    <view class="scene-grid">
      <view class="scene-btn card" :class="{ disabled: !current }" @tap="runScene('lesson_on')">
        <text class="scene-name">上课模式</text>
        <text class="scene-desc">开灯 · 综合屏 · 班牌</text>
      </view>
      <view class="scene-btn card" :class="{ disabled: !current }" @tap="runScene('lesson_off')">
        <text class="scene-name">离开模式</text>
        <text class="scene-desc">关灯 · 关屏 · 关班牌</text>
      </view>
    </view>

    <!-- 实时环境 -->
    <view class="section-title"><text class="t">实时环境</text></view>
    <view v-if="env" class="card env-grid">
      <view class="env-item">
        <text class="env-num">{{ env.temperature != null ? env.temperature.toFixed(1) : '--' }}</text>
        <text class="env-label">温度 ℃</text>
      </view>
      <view class="env-item">
        <text class="env-num">{{ env.humidity != null ? env.humidity.toFixed(0) : '--' }}</text>
        <text class="env-label">湿度 %</text>
      </view>
      <view class="env-item">
        <text class="env-num">{{ env.co2 != null ? env.co2.toFixed(0) : '--' }}</text>
        <text class="env-label">CO₂ ppm</text>
      </view>
    </view>
    <view v-else class="card env-empty">该教室暂无环境传感器数据</view>

    <!-- 设备一览 -->
    <view class="section-title">
      <text class="t">设备状态</text>
      <text class="more">在线 {{ onlineCount }} / {{ devices.length }} 台</text>
    </view>
    <view v-if="devices.length" class="list-group">
      <view v-for="d in devices" :key="d.device_id" class="list-cell list-cell-center">
        <view class="d-icon"><uni-icons :type="deviceTypeIcon(d.device_type, d.zb_type)" size="22" color="var(--text-2)" /></view>
        <view class="d-main">
          <view class="d-title">
            <text class="d-label">{{ d.label }}</text>
            <StatusTag :text="d.status === 'online' ? '在线' : '离线'" :tone="d.status === 'online' ? 'success' : 'danger'" />
          </view>
          <text class="d-sub">{{ deviceType(d.device_type, d.zb_type) }} · {{ d.device_id }}</text>
        </view>
      </view>
    </view>
    <EmptyState v-else title="该教室暂无设备" desc="请稍后重试或切换教室" />

    <!-- 今日进出入口（数据源待接入边侧） -->
    <view class="section-title"><text class="t">今日进出</text></view>
    <view class="card access-entry" @tap="onAccessTap">
      <view class="a-icon"><uni-icons type="flag" size="22" color="var(--text-2)" /></view>
      <view class="a-main">
        <text class="a-label">今日进出记录</text>
        <text class="a-sub">人脸识别登录列表</text>
      </view>
      <uni-icons type="arrow-right" size="14" color="var(--text-4)" />
    </view>

    <AppTabBar current="classroom" />
  </view>
</template>

<script>
import { api } from '../../api/index'
import { store, isLoggedIn } from '../../store/index'
import { deviceTypeLabel } from '../../utils/format'
import { deviceTypeIcon } from '../../utils/icon'
import { controlErrorMessage } from '../../utils/controlError'
import { parseSensorApp } from '../../utils/env'
import { connectWs, closeWs } from '../../utils/ws'
import StatusTag from '../../components/StatusTag.vue'
import EmptyState from '../../components/EmptyState.vue'
import AppTabBar from '../../components/AppTabBar.vue'
import SubscribePrompt from '../../components/SubscribePrompt.vue'

export default {
  components: { StatusTag, EmptyState, AppTabBar, SubscribePrompt },
  data() {
    return {
      store,
      spaces: [],
      current: '',
      devices: [],
      env: null,
      redirecting: false,
    }
  },
  computed: {
    currentSpace() {
      return this.spaces.find((s) => s.space_id === this.current) || this.spaces[0] || null
    },
    onlineCount() {
      return this.devices.filter((d) => d.status === 'online').length
    },
  },
  onShow() {
    if (!isLoggedIn()) {
      uni.reLaunch({ url: '/pages/login/login' })
      return
    }
    // 管理员保持总览首页；此处仅在误入时回跳，用标志位避免 onShow 重复触发。
    if (store.role === 'admin' && !this.redirecting) {
      this.redirecting = true
      uni.reLaunch({ url: '/pages/home/home' })
      return
    }
    this.load()
  },
  onHide() {
    closeWs()
  },
  onUnload() {
    closeWs()
  },
  onPullDownRefresh() {
    this.fetchDevices()
  },
  methods: {
    deviceType: deviceTypeLabel,
    deviceTypeIcon,
    async runScene(sceneId) {
      if (!this.current) {
        uni.showToast({ title: '请先选择教室', icon: 'none' })
        return
      }
      try {
        const res = await api.sceneExecute({ space_id: this.current, scene_id: sceneId })
        if (res && res.warning) {
          uni.showToast({ title: res.warning, icon: 'none' })
          return
        }
        const n = res && res.applied != null ? res.applied : 0
        uni.showToast({ title: `已下发 ${n} 台设备`, icon: 'none' })
      } catch (e) {
        uni.showToast({ title: controlErrorMessage(e) || '下发失败', icon: 'none' })
      }
    },
    async load() {
      try {
        const spaces = await api.spaces()
        this.spaces = spaces
        if (!this.current || !spaces.some((s) => s.space_id === this.current)) {
          this.current = spaces.length ? spaces[0].space_id : ''
        }
        await this.fetchDevices()
        this.openWs()
      } catch (e) {
        uni.showToast({ title: e.message || '加载失败', icon: 'none' })
        uni.stopPullDownRefresh()
      }
    },
    async fetchDevices() {
      if (!this.current) {
        this.devices = []
        this.env = null
        uni.stopPullDownRefresh()
        return
      }
      try {
        const r = await api.devices(this.current)
        const ds = r.devices || []
        this.devices = ds
        const sensor = ds.find(
          (d) => d.device_type === 'zigbee' && d.zb_type === 'sensor' && parseSensorApp(d)
        )
        this.env = sensor ? parseSensorApp(sensor) : null
      } catch (e) {
        uni.showToast({ title: e.message || '加载失败', icon: 'none' })
      } finally {
        uni.stopPullDownRefresh()
      }
    },
    onSpaceChange(e) {
      const idx = Number(e.detail.value)
      const s = this.spaces[idx]
      if (s && s.space_id !== this.current) {
        this.current = s.space_id
        this.fetchDevices()
        this.openWs()
      }
    },
    onAccessTap() {
      const sid = this.current || ''
      const qs = []
      if (sid) qs.push('space_id=' + encodeURIComponent(sid))
      qs.push('auth_type=face')
      uni.navigateTo({ url: '/pages/records/records?' + qs.join('&') })
    },
    openWs() {
      connectWs(this.current, { onMessage: (msg) => this.applyWsMessage(msg) })
    },
    applyWsMessage(msg) {
      if (!msg) return
      if (msg.type === 'device_update') {
        const idx = this.devices.findIndex((d) => d.device_id === msg.device_id)
        if (idx >= 0) {
          this.devices[idx] = { ...this.devices[idx], status: msg.status, app: msg.app }
          // 环境传感器状态更新时同步刷新环境数据
          if (this.devices[idx].device_type === 'zigbee' && this.devices[idx].zb_type === 'sensor') {
            this.env = parseSensorApp(this.devices[idx])
          }
        }
      }
    },
  },
}
</script>

<style scoped>
.classroom {
  padding-top: 20rpx;
  padding-left: 24rpx;
  padding-right: 24rpx;
}

/* 空间选择 */
.space-select {
  display: flex;
  align-items: center;
  justify-content: space-between;
  height: 80rpx;
  padding: 0 24rpx;
  margin-bottom: 8rpx;
}
.space-select-label {
  font-size: 28rpx;
  color: var(--text-1);
}
.space-select-arrow {
  display: flex;
  align-items: center;
}

/* 场景快捷控制 */
.scene-grid {
  display: flex;
}
.scene-btn {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  padding: 28rpx 0;
}
.scene-btn + .scene-btn {
  margin-left: 20rpx;
}
.scene-btn.disabled {
  opacity: 0.5;
}
.scene-name {
  font-size: 28rpx;
  font-weight: 600;
  color: var(--text-1);
}
.scene-desc {
  margin-top: 8rpx;
  font-size: 22rpx;
  color: var(--text-3);
}

/* 环境数据 */
.env-grid {
  display: flex;
}
.env-item {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  padding: 24rpx 0;
}
.env-item + .env-item {
  border-left: 1rpx solid var(--border-lighter);
}
.env-num {
  font-size: 40rpx;
  font-weight: 600;
  color: var(--primary);
  line-height: 1;
}
.env-label {
  margin-top: 12rpx;
  font-size: 22rpx;
  color: var(--text-3);
}
.env-empty {
  padding: 32rpx 24rpx;
  font-size: 24rpx;
  color: var(--text-3);
  text-align: center;
}

/* 设备列表 */
.list-group {
  margin-top: 4rpx;
}
.d-icon {
  width: 80rpx;
  height: 80rpx;
  border-radius: 8rpx;
  margin-right: 22rpx;
  flex-shrink: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  color: var(--text-2);
  background: var(--info-bg);
}
.d-main {
  flex: 1;
  display: flex;
  flex-direction: column;
}
.d-title {
  display: flex;
  align-items: center;
}
.d-label {
  font-size: 30rpx;
  font-weight: 600;
  color: var(--text-1);
  margin-right: 14rpx;
}
.d-sub {
  margin-top: 6rpx;
  font-size: 22rpx;
  color: var(--text-3);
}

/* 今日进出入口 */
.access-entry {
  display: flex;
  align-items: center;
  padding: 24rpx;
}
.a-icon {
  width: 80rpx;
  height: 80rpx;
  border-radius: 8rpx;
  margin-right: 22rpx;
  flex-shrink: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  color: var(--text-2);
  background: var(--info-bg);
}
.a-main {
  flex: 1;
  display: flex;
  flex-direction: column;
}
.a-label {
  font-size: 28rpx;
  color: var(--text-1);
}
.a-sub {
  margin-top: 4rpx;
  font-size: 22rpx;
  color: var(--text-3);
}
</style>
