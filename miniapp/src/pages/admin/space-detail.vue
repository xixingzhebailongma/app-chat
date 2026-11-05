<template>
  <view class="sd page-pad">
    <!-- 教室信息卡：名称/类型在此编辑，启用状态直接切换 -->
    <view v-if="space" class="card info-card">
      <view class="info-head">
        <view class="info-main">
          <text class="info-name">{{ space.name }}</text>
          <text class="info-sub">{{ typeName(space.type) }} · {{ space.space_id }}</text>
        </view>
        <view class="info-enable" @tap.stop>
          <text class="info-enable-label">{{ space.enabled ? '已启用' : '已停用' }}</text>
          <switch class="info-switch" :checked="space.enabled" color="#245e70" @change="onToggleEnabled" />
        </view>
      </view>
      <view class="info-actions">
        <view class="act-btn" @tap="openEdit">
          <uni-icons type="compose" size="14" color="var(--text-2)" />
          <text class="act-label">编辑教室信息</text>
        </view>
        <view class="act-btn" @tap="load">
          <uni-icons type="refreshicon" size="14" color="var(--text-2)" />
          <text class="act-label">刷新设备</text>
        </view>
      </view>
      <text class="info-stat">{{ statsText }}</text>
    </view>

    <!-- 可控设备：开关类 / 环境面板，点「控制」下发开/关/切换 -->
    <view v-if="controlDevices.length" class="card dev-card">
      <view class="card-title">可控设备<text class="card-more">{{ controlDevices.length }} 台</text></view>
      <view
        v-for="d in controlDevices"
        :key="d.device_id"
        class="dev-row"
        :class="{ 'dev-row--off': d.status !== 'online' }"
      >
        <view class="dev-icon" :style="d.status === 'online' ? { background: 'var(--brand-soft)' } : {}">
          <uni-icons
            :type="deviceTypeIcon(d.device_type, d.zb_type)"
            size="20"
            :color="d.status === 'online' ? 'var(--brand)' : 'var(--text-4)'"
          />
        </view>
        <view class="dev-main">
          <text class="dev-label">{{ d.label }}</text>
          <text class="dev-sub">{{ devStateText(d) }}</text>
        </view>
        <view class="dev-act">
          <button class="ctrl-btn" :disabled="d.status !== 'online'" @tap="openControl(d)">控制</button>
        </view>
      </view>
    </view>

    <!-- 传感器：只读，展示实时读数 -->
    <view v-if="sensorDevices.length" class="card dev-card">
      <view class="card-title">传感器<text class="card-more">只读 · 实时上报</text></view>
      <view v-for="d in sensorDevices" :key="d.device_id" class="dev-row" :class="{ 'dev-row--off': d.status !== 'online' }">
        <view class="dev-icon" :style="d.status === 'online' ? { background: 'var(--brand-soft)' } : {}">
          <uni-icons
            type="cloud-upload"
            size="20"
            :color="d.status === 'online' ? 'var(--brand)' : 'var(--text-4)'"
          />
        </view>
        <view class="dev-main">
          <text class="dev-label">{{ d.label }}</text>
          <text class="dev-sub">{{ sensorText(d) }}</text>
        </view>
      </view>
    </view>

    <EmptyState
      v-if="!loading && !devices.length"
      title="该教室暂无设备"
      desc="等边侧装好设备后会自动出现"
      icon="flag"
    />

    <!-- 控制弹层 -->
    <ActionSheet
      :visible="controlVisible"
      :title="controlTarget ? controlTarget.label : ''"
      :sub="controlTarget ? deviceType(controlTarget.device_type, controlTarget.zb_type) + ' · ' + controlTarget.device_id : ''"
      @close="controlVisible = false"
    >
      <view class="cmd-grid">
        <view v-for="c in commands" :key="c.cmd" class="cmd" @tap="sendCommand(c)">
          <text class="cmd-label">{{ c.label }}</text>
        </view>
      </view>
      <text class="cmd-hint">下发成功仅代表指令已发出，最终状态以设备上报为准</text>
    </ActionSheet>

    <!-- 编辑教室信息（名称 / 类型） -->
    <ActionSheet :visible="editVisible" title="编辑教室信息" @close="editVisible = false">
      <view class="field-row">
        <text class="field-label">教室名</text>
        <input v-model="form.name" class="field" placeholder="如 共用教室 / A201" placeholder-class="ph" />
      </view>
      <view class="field-row">
        <text class="field-label">类型</text>
        <picker mode="selector" :range="typeOptions" range-key="label" @change="onTypeChange">
          <view class="field pick-field">
            <text :class="form.type ? 'pick-val' : 'ph'">{{ typeLabel }}</text>
            <uni-icons type="arrow-down" size="16" color="var(--text-3)" />
          </view>
        </picker>
      </view>
      <button class="btn btn-primary" :disabled="submitting || !canSubmit" @tap="confirmEdit">保存</button>
    </ActionSheet>
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
import EmptyState from '../../components/EmptyState.vue'
import ActionSheet from '../../components/ActionSheet.vue'

export default {
  components: { EmptyState, ActionSheet },
  data() {
    return {
      spaceId: '',
      space: null,
      typeDefs: [],
      devices: [],
      doors: [],
      loading: true,
      controlVisible: false,
      controlTarget: null,
      editVisible: false,
      form: { name: '', type: '' },
      submitting: false,
      redirecting: false,
    }
  },
  computed: {
    isAdmin() {
      return store.role === 'admin'
    },
    typeOptions() {
      return this.typeDefs.map((t) => ({ key: t.code, label: t.name }))
    },
    typeLabel() {
      const t = this.typeDefs.find((x) => x.code === this.form.type)
      return t ? t.name : '请选择类型'
    },
    canSubmit() {
      return !!(this.form.name.trim() && this.form.type)
    },
    /* 可控设备：非传感器（开关 zigbee / 综合屏等） */
    controlDevices() {
      return this.devices.filter((d) => !(d.device_type === 'zigbee' && d.zb_type === 'sensor'))
    },
    sensorDevices() {
      return this.devices.filter((d) => d.device_type === 'zigbee' && d.zb_type === 'sensor')
    },
    statsText() {
      if (this.loading) return '设备加载中…'
      const online = this.devices.filter((d) => d.status === 'online').length
      return this.devices.length + ' 台设备 · ' + online + ' 台在线'
    },
    commands() {
      const d = this.controlTarget
      if (!d) return []
      if (d.device_type === 'zigbee' && d.zb_type === 'sensor') return []
      if (d.device_type === 'zigbee') {
        if (this.isDoor(d)) {
          return [
            { cmd: 'on', label: '锁门' },
            { cmd: 'off', label: '开门' },
          ]
        }
        return [
          { cmd: 'on', label: '开' },
          { cmd: 'off', label: '关' },
          { cmd: 'toggle', label: '切换' },
        ]
      }
      return [
        { cmd: 'on', label: '开' },
        { cmd: 'off', label: '关' },
        { cmd: 'toggle', label: '切换' },
      ]
    },
  },
  onLoad(options) {
    this.spaceId = (options && options.space_id) || ''
  },
  onShow() {
    if (!isLoggedIn()) {
      uni.reLaunch({ url: '/pages/login/login' })
      return
    }
    if (!this.isAdmin && !this.redirecting) {
      this.redirecting = true
      uni.reLaunch({ url: '/pages/home/home' })
      return
    }
    this.load()
  },
  onPullDownRefresh() {
    this.load()
  },
  onUnload() {
    closeWs()
  },
  methods: {
    deviceType: deviceTypeLabel,
    deviceTypeIcon,
    typeName(code) {
      const t = this.typeDefs.find((x) => x.code === code)
      return t ? t.name : code
    },
    isDoor(d) {
      return this.doors.some((x) => x.space_id === d.space_id && x.device_id === d.device_id)
    },
    /* 设备当前状态：在线设备读 app 的 on/off，其余显示离线 */
    devStateText(d) {
      if (d.status !== 'online') return '设备已离线'
      if (d.app === 'on' || d.app === '1' || d.app === true) return '在线 · 开'
      if (d.app === 'off' || d.app === '0' || d.app === false) return '在线 · 关'
      return '在线'
    },
    sensorText(d) {
      if (d.status !== 'online') return '设备已离线'
      const o = parseSensorApp(d)
      if (!o) return '在线'
      const unit = { temperature: '℃', humidity: '%RH', co2: 'ppm', light: 'lux', pm25: 'μg/m³' }
      const name = { temperature: '温度', humidity: '湿度', co2: 'CO₂', light: '光照', pm25: 'PM2.5' }
      const parts = Object.keys(o).map((k) => {
        if (k === 'pir') return '人体感应 ' + (o[k] ? '有人' : '无人')
        return name[k] + ' ' + o[k] + (unit[k] || '')
      })
      return parts.join(' · ')
    },
    async load() {
      try {
        const reqs = [api.adminSpaces(), api.spaceTypes(), api.doorDevices(this.spaceId)]
        if (this.spaceId) reqs.push(api.devices(this.spaceId))
        const [spaces, typeDefs, doorRes, devRes] = await Promise.all(reqs)
        this.typeDefs = typeDefs || []
        this.space = (spaces || []).find((s) => s.space_id === this.spaceId) || null
        this.doors = (doorRes && doorRes.doors) || []
        this.devices = ((devRes && devRes.devices) || []).map((d) => ({ ...d, space_id: this.spaceId }))
        this.openWs()
      } catch (e) {
        uni.showToast({ title: e.message || '加载失败', icon: 'none' })
      } finally {
        this.loading = false
        uni.stopPullDownRefresh()
      }
    },
    openWs() {
      connectWs(this.spaceId, { onMessage: (msg) => this.applyWsMessage(msg) })
    },
    /* device_update 是权威状态：按 device_id 回填在线状态与 app */
    applyWsMessage(msg) {
      if (!msg || msg.type !== 'device_update') return
      const idx = this.devices.findIndex((d) => d.device_id === msg.device_id)
      if (idx >= 0) {
        this.devices[idx] = { ...this.devices[idx], status: msg.status, app: msg.app }
      }
    },
    openEdit() {
      if (!this.space) return
      this.form = { name: this.space.name, type: this.space.type }
      this.submitting = false
      this.editVisible = true
    },
    onTypeChange(e) {
      const t = this.typeOptions[Number(e.detail.value)]
      if (t) this.form.type = t.key
    },
    async confirmEdit() {
      if (this.submitting || !this.canSubmit) return
      this.submitting = true
      try {
        await api.updateSpace(this.spaceId, { name: this.form.name.trim(), type: this.form.type })
        this.editVisible = false
        uni.showToast({ title: '已保存', icon: 'success' })
        this.load()
      } catch (e) {
        uni.showToast({ title: e.message || '保存失败', icon: 'none' })
      } finally {
        this.submitting = false
      }
    },
    async onToggleEnabled(e) {
      const enabled = !!e.detail.value
      try {
        await api.updateSpace(this.spaceId, { enabled })
        if (this.space) this.space.enabled = enabled
      } catch (err) {
        uni.showToast({ title: err.message || '操作失败', icon: 'none' })
      }
    },
    openControl(d) {
      this.controlTarget = d
      this.controlVisible = true
    },
    async sendCommand(c) {
      const d = this.controlTarget
      try {
        await this.doControl(d, c.cmd, false)
        this.controlVisible = false
        uni.showToast({ title: '已发送，等待设备响应', icon: 'none' })
      } catch (e) {
        if (e && e.code === 'CONFIRM_REQUIRED') {
          uni.showModal({
            title: '确认开门',
            content: '即将对「' + d.label + '」执行开门，请确认',
            confirmText: '确认开门',
            success: async (res) => {
              if (!res.confirm) return
              try {
                await this.doControl(d, c.cmd, true)
                this.controlVisible = false
                uni.showToast({ title: '已发送，等待设备响应', icon: 'none' })
              } catch (e2) {
                uni.showToast({ title: controlErrorMessage(e2) || '下发失败', icon: 'none' })
              }
            },
          })
        } else {
          uni.showToast({ title: controlErrorMessage(e) || '下发失败', icon: 'none' })
        }
      }
    },
    doControl(d, command, confirm) {
      return api.control({
        space_id: d.space_id,
        device_type: d.device_type,
        device_id: d.device_id,
        command,
        confirm,
      })
    },
  },
}
</script>

<style scoped>
.sd {
  padding-top: 24rpx;
  padding-left: 36rpx;
  padding-right: 36rpx;
  padding-bottom: calc(40rpx + env(safe-area-inset-bottom));
}

/* 教室信息卡 */
.info-card {
  padding: 32rpx;
}
.info-head {
  display: flex;
  align-items: center;
  justify-content: space-between;
}
.info-main {
  flex: 1;
  min-width: 0;
  display: flex;
  flex-direction: column;
}
.info-name {
  font-size: 34rpx;
  font-weight: 700;
  color: var(--text-1);
}
.info-sub {
  margin-top: 6rpx;
  font-size: 22rpx;
  color: var(--text-3);
}
.info-enable {
  display: flex;
  align-items: center;
  flex-shrink: 0;
}
.info-enable-label {
  margin-right: 8rpx;
  font-size: 24rpx;
  color: var(--text-3);
}
.info-switch {
  transform: scale(0.8);
  transform-origin: right center;
}
.info-actions {
  display: flex;
  align-items: center;
  margin-top: 20rpx;
}
.info-stat {
  display: block;
  margin-top: 16rpx;
  font-size: 22rpx;
  color: var(--text-3);
}

/* 设备卡 */
.dev-card {
  padding: 32rpx;
}
.card-title {
  display: flex;
  align-items: baseline;
  margin-bottom: 8rpx;
  font-size: 28rpx;
  font-weight: 600;
  color: var(--text-1);
}
.card-more {
  margin-left: auto;
  font-size: 22rpx;
  font-weight: 400;
  color: var(--text-3);
}
.dev-row {
  display: flex;
  align-items: center;
  padding: 22rpx 0;
  border-bottom: 1rpx solid var(--border-light);
}
.dev-row:last-child {
  border-bottom: none;
}
.dev-row--off .dev-label,
.dev-row--off .dev-sub {
  color: var(--text-4);
}
.dev-icon {
  width: 68rpx;
  height: 68rpx;
  flex-shrink: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  border-radius: 18rpx;
  background: var(--bg);
}
.dev-main {
  flex: 1;
  min-width: 0;
  margin-left: 20rpx;
  display: flex;
  flex-direction: column;
  gap: 6rpx;
}
.dev-label {
  font-size: 28rpx;
  font-weight: 600;
  color: var(--text-1);
}
.dev-sub {
  font-size: 22rpx;
  color: var(--text-3);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.dev-act {
  flex-shrink: 0;
  margin-left: 16rpx;
}
.ctrl-btn {
  height: 60rpx;
  padding: 0 26rpx;
  line-height: 60rpx;
  font-size: 24rpx;
  color: #fff;
  background: var(--primary);
  border-radius: 999rpx;
}
.ctrl-btn::after {
  border: none;
}
.ctrl-btn[disabled] {
  background: var(--primary-soft);
  color: var(--text-4);
}

/* 控制命令网格 */
.cmd-grid {
  display: flex;
  margin: 8rpx 0 4rpx;
}
.cmd {
  flex: 1;
  margin-right: 16rpx;
  height: 100rpx;
  display: flex;
  align-items: center;
  justify-content: center;
  background: var(--info-bg);
  border: 1rpx solid var(--info-border);
  border-radius: 14rpx;
}
.cmd:last-child {
  margin-right: 0;
}
.cmd-label {
  font-size: 28rpx;
  color: var(--text-1);
  font-weight: 600;
}
.cmd-hint {
  display: block;
  margin-top: 16rpx;
  font-size: 22rpx;
  color: var(--text-4);
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
  border-radius: 12rpx;
  font-size: 30rpx;
}
.pick-field {
  display: flex;
  align-items: center;
  justify-content: space-between;
}
.pick-val {
  font-size: 30rpx;
  color: var(--text-1);
}
.ph {
  color: var(--text-4);
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
.act-btn {
  display: flex;
  align-items: center;
  height: 56rpx;
  padding: 0 18rpx;
  margin: 0 12rpx 0 0;
  background: var(--info-bg);
  border: 1rpx solid var(--info-border);
  border-radius: 12rpx;
}
.act-label {
  margin-left: 6rpx;
  font-size: 24rpx;
  color: var(--text-2);
}
</style>
