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
    <view class="section-title">
      <text class="t">场景快捷控制</text>
      <text class="more more-action" @tap="openCreateScene">+ 添加场景</text>
    </view>
    <view v-if="scenes.length" class="scene-grid">
      <view
        v-for="s in scenes"
        :key="s.scene_id"
        class="scene-btn card"
        :class="{ active: s.scene_id === activeSceneId }"
        @tap="runScene(s)"
        @longpress="onSceneLongPress(s)"
      >
        <text class="scene-name">{{ s.name }}</text>
        <text class="scene-desc">{{ sceneDesc(s) }}</text>
      </view>
    </view>
    <EmptyState v-else title="暂无场景" desc="点右上角「添加场景」创建" icon="flag" />

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

    <!-- 设备一览（点击跳设备控制） -->
    <view class="section-title">
      <text class="t">设备状态</text>
      <text class="more">在线 {{ onlineCount }} / {{ devices.length }} 台</text>
    </view>
    <view v-if="devices.length" class="list-group">
      <view v-for="d in devices" :key="d.device_id" class="list-cell list-cell-center" hover-class="cell-hover" @tap="onDeviceTap(d)">
        <view class="d-icon"><uni-icons :type="deviceTypeIcon(d.device_type, d.zb_type)" size="22" color="var(--text-2)" /></view>
        <view class="d-main">
          <view class="d-title">
            <text class="d-label">{{ d.label }}</text>
            <StatusTag :text="d.status === 'online' ? '在线' : '离线'" :tone="d.status === 'online' ? 'success' : 'danger'" />
          </view>
          <text class="d-sub">{{ deviceType(d.device_type, d.zb_type) }} · {{ d.device_id }}</text>
        </view>
        <view v-if="isSwitchable(d)" class="d-switch-wrap" @tap.stop>
          <switch class="d-switch" :checked="d.app === 'on'" color="#5b8def" @change="onDeviceToggle(d, $event)" />
        </view>
        <uni-icons type="arrow-right" size="14" color="var(--text-4)" />
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

    <!-- 场景编辑弹层 -->
    <view v-if="sceneSheetVisible" class="mask" @tap="sceneSheetVisible = false"></view>
    <view class="sheet" :class="{ show: sceneSheetVisible }">
      <view class="sheet-head">
        <text class="sheet-title">{{ editingScene ? '编辑场景' : '新建场景' }}</text>
        <view class="sheet-close" @tap="sceneSheetVisible = false">
          <uni-icons type="close" size="18" color="var(--text-4)" />
        </view>
      </view>
      <view class="field-row">
        <text class="field-label">场景名</text>
        <input v-model="sceneForm.name" class="field" placeholder="如 午休模式 / 考试模式" placeholder-class="ph" />
      </view>
      <view class="section-title"><text class="t">设备状态</text></view>
      <text class="sd-hint">仅开关类设备可加入场景；门禁、传感器、编码器不纳入</text>
      <view v-if="sceneControllableDevices.length" class="list-group scene-device-list">
        <view v-for="d in sceneControllableDevices" :key="d.device_id" class="list-cell list-cell-center">
          <text class="sd-label">{{ d.label }}</text>
          <switch :checked="!!sceneForm.states[d.device_id]" color="#5b8def" @change="onSceneDeviceToggle(d, $event)" />
        </view>
      </view>
      <text v-else class="sd-empty">该教室暂无开关类设备</text>
      <view class="sheet-footer">
        <button class="btn btn-primary" :disabled="submitting" @tap="saveScene">确认保存</button>
      </view>
    </view>
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
      scenes: [],
      activeSceneId: '',
      sceneSheetVisible: false,
      editingScene: null,
      sceneForm: { name: '', states: {} },
      submitting: false,
    }
  },
  computed: {
    currentSpace() {
      return this.spaces.find((s) => s.space_id === this.current) || this.spaces[0] || null
    },
    onlineCount() {
      return this.devices.filter((d) => d.status === 'online').length
    },
    sceneControllableDevices() {
      return this.devices.filter((d) => this.isSwitchable(d))
    },
    canSaveScene() {
      return !!(this.sceneForm.name.trim())
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
    deviceLabel(id) {
      const d = this.devices.find((x) => x.device_id === id)
      return d ? d.label : id
    },
    isSwitchable(d) {
      if (d.device_type === 'zigbee') return d.zb_type === 'switch' && d.zb_role !== 'door'
      return d.device_type === 'fuhe-screen' || d.device_type === 'fuhe-board'
    },
    sceneDesc(s) {
      if (s.kind === 'all_on') return '全部开启'
      if (s.kind === 'all_off') return '全部关闭'
      const on = (s.device_states || []).filter((x) => x.command === 'on').map((x) => this.deviceLabel(x.device_id))
      if (on.length) return '开 ' + on.join('、')
      return '全部关闭'
    },
    async runScene(scene) {
      if (!this.current) {
        uni.showToast({ title: '请先选择教室', icon: 'none' })
        return
      }
      try {
        const res = await api.executeScene(scene.scene_id)
        if (res && res.warning) {
          uni.showToast({ title: res.warning, icon: 'none' })
          return
        }
        const n = res && res.applied != null ? res.applied : 0
        this.activeSceneId = scene.scene_id
        uni.showToast({ title: `已下发 ${n} 台设备`, icon: 'none' })
        this.fetchDevices()  // 重新拉设备，反映开关态变化
      } catch (e) {
        uni.showToast({ title: controlErrorMessage(e) || '下发失败', icon: 'none' })
      }
    },
    openCreateScene() {
      this.editingScene = null
      this.sceneForm = { name: '', states: {} }
      this.submitting = false
      this.sceneSheetVisible = true
    },
    openEditScene(scene) {
      this.editingScene = scene
      const states = {}
      ;(scene.device_states || []).forEach((s) => { states[s.device_id] = s.command === 'on' })
      this.sceneForm = { name: scene.name, states }
      this.submitting = false
      this.sceneSheetVisible = true
    },
    onSceneLongPress(scene) {
      uni.showActionSheet({
        itemList: ['编辑场景', '删除场景'],
        success: (res) => {
          if (res.tapIndex === 0) {
            this.openEditScene(scene)
          } else if (res.tapIndex === 1) {
            this.removeScene(scene)
          }
        },
      })
    },
    onSceneDeviceToggle(d, e) {
      this.sceneForm.states[d.device_id] = !!e.detail.value
    },
    async saveScene() {
      if (this.submitting) return
      if (!this.sceneForm.name.trim()) {
        uni.showToast({ title: '请输入场景名', icon: 'none' })
        return
      }
      this.submitting = true
      try {
        const deviceStates = this.sceneControllableDevices.map((d) => ({
          device_id: d.device_id,
          device_type: d.device_type,
          command: this.sceneForm.states[d.device_id] ? 'on' : 'off',
        }))
        if (this.editingScene) {
          await api.updateScene(this.editingScene.scene_id, { name: this.sceneForm.name.trim(), device_states: deviceStates })
        } else {
          await api.createScene({ space_id: this.current, name: this.sceneForm.name.trim(), device_states: deviceStates })
        }
        this.sceneSheetVisible = false
        uni.showToast({ title: '已保存', icon: 'success' })
        this.loadScenes()
      } catch (e) {
        uni.showToast({ title: e.message || '保存失败', icon: 'none' })
      } finally {
        this.submitting = false
      }
    },
    removeScene(scene) {
      uni.showModal({
        title: '删除场景',
        content: `确定删除「${scene.name}」？`,
        success: async (res) => {
          if (!res.confirm) return
          try {
            await api.deleteScene(scene.scene_id)
            if (this.activeSceneId === scene.scene_id) this.activeSceneId = ''
            uni.showToast({ title: '已删除', icon: 'success' })
            this.loadScenes()
          } catch (e) {
            uni.showToast({ title: e.message || '删除失败', icon: 'none' })
          }
        },
      })
    },
    async load() {
      try {
        const spaces = await api.spaces()
        this.spaces = spaces
        if (!this.current || !spaces.some((s) => s.space_id === this.current)) {
          this.current = spaces.length ? spaces[0].space_id : ''
        }
        await this.fetchDevices()
        await this.loadScenes()
        this.openWs()
      } catch (e) {
        uni.showToast({ title: e.message || '加载失败', icon: 'none' })
        uni.stopPullDownRefresh()
      }
    },
    async loadScenes() {
      if (!this.current) {
        this.scenes = []
        this.activeSceneId = ''
        return
      }
      try {
        const res = await api.scenes(this.current)
        this.scenes = (res && res.scenes) || []
        this.activeSceneId = (res && res.active_scene_id) || ''
      } catch (e) {
        uni.showToast({ title: e.message || '场景加载失败', icon: 'none' })
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
        this.loadScenes()
        this.openWs()
      }
    },
    onDeviceTap(d) {
      const qs = 'space_id=' + encodeURIComponent(this.current) + '&device_id=' + encodeURIComponent(d.device_id)
      uni.navigateTo({ url: '/pages/devices/devices?' + qs })
    },
    async onDeviceToggle(d, e) {
      const cmd = e.detail.value ? 'on' : 'off'
      try {
        await api.control({ space_id: this.current, device_type: d.device_type, device_id: d.device_id, command: cmd })
        d.app = cmd  // 本地立即更新开关态
        this.activeSceneId = ''  // 手动控制后激活场景失效（状态已偏离该场景）
      } catch (err) {
        uni.showToast({ title: controlErrorMessage(err) || '控制失败', icon: 'none' })
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
.more-action {
  color: var(--primary);
}
.scene-grid {
  display: flex;
  flex-wrap: wrap;
  margin: 0 -8rpx;
}
.scene-btn {
  width: calc(50% - 16rpx);
  margin: 0 8rpx 16rpx;
  box-sizing: border-box;
  display: flex;
  flex-direction: column;
  align-items: center;
  padding: 28rpx 12rpx;
  border: 1rpx solid var(--border);
}
.scene-btn.active {
  background: rgba(91, 141, 239, 0.10);
  border-color: rgba(91, 141, 239, 0.4);
}
.scene-btn.active .scene-name {
  color: var(--primary);
}
.scene-btn.active .scene-desc {
  color: var(--primary);
  opacity: 0.85;
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
  text-align: center;
  line-height: 1.4;
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
.d-switch-wrap {
  flex-shrink: 0;
  margin-left: 12rpx;
}
.d-switch {
  transform: scale(0.7);
  transform-origin: right center;
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

/* 场景编辑弹层 */
.mask {
  position: fixed;
  left: 0;
  right: 0;
  top: 0;
  bottom: 0;
  z-index: 1000;
  background: rgba(0, 0, 0, 0.5);
}
.sheet {
  position: fixed;
  left: 0;
  right: 0;
  bottom: 0;
  z-index: 1001;
  background: #fff;
  border-radius: 16rpx 16rpx 0 0;
  padding: 32rpx 32rpx 0;
  transform: translateY(100%);
  transition: transform 0.25s ease;
  max-height: 85vh;
  overflow-y: auto;
}
.sheet-footer {
  position: sticky;
  bottom: 0;
  background: #fff;
  padding: 16rpx 0 calc(24rpx + env(safe-area-inset-bottom));
}
.sheet.show {
  transform: translateY(0);
}
.sheet-head {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 16rpx;
}
.sheet-title {
  font-size: 34rpx;
  font-weight: 600;
  color: var(--text-1);
}
.sheet-close {
  display: flex;
  align-items: center;
  padding: 8rpx;
}
.field-row {
  margin-bottom: 20rpx;
}
.field-label {
  display: block;
  margin-bottom: 12rpx;
  font-size: 24rpx;
  color: var(--text-3);
}
.field {
  height: 92rpx;
  padding: 0 28rpx;
  background: var(--bg);
  border: 1rpx solid var(--border);
  border-radius: 8rpx;
  font-size: 30rpx;
}
.ph {
  color: var(--text-4);
}
.scene-device-list {
}
.sd-label {
  flex: 1;
  font-size: 28rpx;
  color: var(--text-1);
}
.sd-hint {
  display: block;
  margin: 4rpx 0 8rpx;
  font-size: 22rpx;
  color: var(--text-4);
}
.sd-empty {
  display: block;
  padding: 24rpx 0;
  font-size: 24rpx;
  color: var(--text-4);
  text-align: center;
}
.btn {
  margin-top: 8rpx;
}
.btn-primary {
  background: var(--primary);
  color: #fff;
}
.btn-primary[disabled] {
  background: var(--primary-soft);
  color: var(--text-4);
}
.btn-primary::after {
  border: none;
}
</style>
