<template>
  <view class="devices page-pad">
    <!-- 空间类型（第一级：标准教室 / 多媒体报告厅 / 办公室） -->
    <view class="type-tabs">
      <view
        v-for="t in typeTabs"
        :key="t.key"
        class="type-tab"
        :class="{ active: currentType === t.key }"
        @tap="onTypeChange(t.key)"
      >{{ t.label }}</view>
    </view>

    <!-- 空间选择（第二级：该类型下的教室） -->
    <picker
      mode="selector"
      :range="spaceTabs"
      range-key="label"
      @change="onSpaceChange"
    >
      <view class="space-select card">
        <text class="space-select-label">{{ currentSpaceLabel }}</text>
        <view class="space-select-arrow"><uni-icons type="arrow-down" size="16" color="var(--text-3)" /></view>
      </view>
    </picker>

    <!-- 搜索 -->
    <view class="search card">
      <view class="search-ico"><uni-icons type="search" size="18" color="var(--text-3)" /></view>
      <input
        v-model="keyword"
        class="search-input"
        placeholder="搜索设备名称 / 编号"
        placeholder-class="ph"
      />
      <view v-if="keyword" class="search-clear" @tap="keyword = ''"><uni-icons type="clear" size="18" color="var(--text-4)" /></view>
    </view>

    <!-- 汇总 -->
    <view class="summary">
      <text>共 <text class="n">{{ devices.length }}</text> 台</text>
      <text class="sep">·</text>
      <view class="ok">
        <view class="dot dot-online"></view>
        <text>在线 <text class="n">{{ onlineCount }}</text></text>
      </view>
      <text class="sep">·</text>
      <view class="off">
        <view class="dot dot-offline"></view>
        <text>离线 <text class="n">{{ offlineCount }}</text></text>
      </view>
    </view>

    <!-- 设备列表 -->
    <view v-if="visibleDevices.length" class="list-group">
      <view
        v-for="d in visibleDevices"
        :key="d.device_id"
        :id="'device-' + d.device_id"
        class="list-cell list-cell-center"
        :class="{ 'device-highlight': d.device_id === highlightDeviceId }"
      >
        <view class="d-icon"><uni-icons :type="deviceTypeIcon(d.device_type, d.zb_type)" size="22" color="var(--text-2)" /></view>
        <view class="d-main">
          <view class="d-title">
            <text class="d-label">{{ d.label }}</text>
            <StatusTag :text="d.status === 'online' ? '在线' : '离线'" :tone="d.status === 'online' ? 'success' : 'danger'" />
          </view>
          <text class="d-sub">{{ deviceType(d.device_type, d.zb_type) }} · {{ d.device_id }}</text>
        </view>
        <button class="ctrl-btn" :disabled="d.status !== 'online' || !canControl(d)" @tap="openControl(d)">控制</button>
      </view>
    </view>
    <EmptyState
      v-else
      :title="keyword ? '未找到匹配设备' : '该空间暂无设备'"
      :desc="keyword ? '请调整关键词后重试' : '请切换其他空间查看'"
    />

    <!-- 控制弹层 -->
    <view v-if="sheetVisible" class="mask" @tap="sheetVisible = false"></view>
    <view class="sheet" :class="{ show: sheetVisible }">
      <view class="sheet-head">
        <view>
          <text class="sheet-title">{{ controlTarget ? controlTarget.label : '' }}</text>
          <text class="sheet-sub">{{ controlTarget ? deviceType(controlTarget.device_type, controlTarget.zb_type) + ' · ' + controlTarget.device_id : '' }}</text>
        </view>
        <view class="sheet-close" @tap="sheetVisible = false"><uni-icons type="close" size="18" color="var(--text-4)" /></view>
      </view>
      <view class="cmd-grid">
        <view
          v-for="c in commands"
          :key="c.cmd"
          class="cmd"
          @tap="sendCommand(c)"
        >
          <text class="cmd-label">{{ c.label }}</text>
        </view>
      </view>
      <!-- 标记门禁（仅 admin、zigbee switch） -->
      <view
        v-if="isAdmin && controlTarget && controlTarget.device_type === 'zigbee' && controlTarget.zb_type === 'switch'"
        class="door-toggle"
        @tap="toggleDoor(controlTarget)"
      >
        <text class="door-toggle-label">标记为门禁（开门需二次确认）</text>
        <view class="door-switch" :class="{ on: isDoor(controlTarget) }"><view class="door-knob"></view></view>
      </view>
    </view>

    <AppTabBar current="devices" />
  </view>
</template>

<script>
import { api } from '../../api/index'
import { store, isLoggedIn } from '../../store/index'
import { deviceTypeLabel, spaceTypeLabel } from '../../utils/format'
import { deviceTypeIcon } from '../../utils/icon'
import { controlErrorMessage } from '../../utils/controlError'
import { connectWs, closeWs } from '../../utils/ws'
import StatusTag from '../../components/StatusTag.vue'
import EmptyState from '../../components/EmptyState.vue'
import AppTabBar from '../../components/AppTabBar.vue'

export default {
  components: { StatusTag, EmptyState, AppTabBar },
  data() {
    return {
      store,
      spaces: [],
      typeDefs: [],
      currentType: '',
      current: 'all',
      requestedType: '',
      requestedSpace: '',
      requestedDevice: '',
      requestedFromAlert: '',
      highlightDeviceId: '',
      highlightTimer: null,
      devices: [],
      doors: [],
      keyword: '',
      sheetVisible: false,
      controlTarget: null,
    }
  },
  computed: {
    isAdmin() {
      return store.role === 'admin'
    },
    typeTabs() {
      // tab 集合来自空间自身 type（教师只看到自己空间里的类型）；label/顺序
      // 优先取服务端 typeDefs，未命中回退 format.js 硬编码。读接口返回含停用，
      // 故停用类型只要还有空间在用就仍显示（不做 enabled 过滤）。
      const codes = [...new Set(this.spaces.map((s) => s.type))]
      const byCode = {}
      this.typeDefs.forEach((t) => { byCode[t.code] = t })
      const known = codes.filter((c) => byCode[c])
      const rest = codes.filter((c) => !byCode[c]).sort()
      known.sort((a, b) => ((byCode[a].sort_order ?? 0) - (byCode[b].sort_order ?? 0)))
      return known.concat(rest).map((c) => ({
        key: c,
        label: byCode[c] ? byCode[c].name : spaceTypeLabel(c),
      }))
    },
    currentTypeLabel() {
      const t = this.typeDefs.find((x) => x.code === this.currentType)
      return t ? t.name : spaceTypeLabel(this.currentType)
    },
    spaceTabs() {
      const rooms = this.spaces.filter((s) => s.type === this.currentType)
      const tabs = []
      if (this.isAdmin) tabs.push({ key: 'all', label: '全部' + this.currentTypeLabel })
      rooms.forEach((s) => tabs.push({ key: s.space_id, label: s.name }))
      return tabs
    },
    currentSpaceLabel() {
      const s = this.spaceTabs.find((x) => x.key === this.current)
      return s ? s.label : '请选择空间'
    },
    onlineCount() {
      return this.devices.filter((d) => d.status === 'online').length
    },
    offlineCount() {
      return this.devices.length - this.onlineCount
    },
    visibleDevices() {
      const k = this.keyword.trim().toLowerCase()
      if (!k) return this.devices
      return this.devices.filter(
        (d) => d.label.toLowerCase().includes(k) || d.device_id.toLowerCase().includes(k)
      )
    },
    commands() {
      const d = this.controlTarget
      if (!d) return []
      if (d.device_type === 'zigbee' && d.zb_type === 'sensor') return []
      if (d.device_type === 'zigbee') {
        // 门禁不支持 toggle（只 on/off），且 on=锁门、off=开门
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
      // fuhe-screen / fuhe-board / fuhe-encoder 等
      return [
        { cmd: 'on', label: '开' },
        { cmd: 'off', label: '关' },
      ]
    },
  },
  onLoad(options) {
    // 从首页点进来带 type（按类型）或 space_id（按教室）参数；
    // 从告警详情「远程控制设备」进来再带 device_id / from_alert 定位高亮。
    this.requestedType = (options && options.type) || ''
    this.requestedSpace = (options && options.space_id) || ''
    this.requestedDevice = (options && options.device_id) || ''
    this.requestedFromAlert = (options && options.from_alert) || ''
  },
  onShow() {
    if (!isLoggedIn()) {
      uni.reLaunch({ url: '/pages/login/login' })
      return
    }
    this.load()
  },
  onHide() {
    if (this.highlightTimer) clearTimeout(this.highlightTimer)
    closeWs()
  },
  onUnload() {
    if (this.highlightTimer) clearTimeout(this.highlightTimer)
    closeWs()
  },
  onPullDownRefresh() {
    this.fetchDevices()
  },
  methods: {
    deviceType: deviceTypeLabel,
    deviceTypeIcon,
    canControl(d) {
      return !(d.device_type === 'zigbee' && d.zb_type === 'sensor')
    },
    isDoor(d) {
      return this.doors.some((x) => x.space_id === d.space_id && x.device_id === d.device_id)
    },
    async load() {
      try {
        const [spaces, typeDefs] = await Promise.all([api.spaces(), api.spaceTypes()])
        this.spaces = spaces
        this.typeDefs = typeDefs || []
        let type = this.typeTabs.length ? this.typeTabs[0].key : ''
        if (this.requestedType && this.typeTabs.some((t) => t.key === this.requestedType)) {
          type = this.requestedType
        }
        this.currentType = type
        this.requestedType = ''
        this.current = !this.isAdmin && spaces.length ? spaces[0].space_id : 'all'
        // 带 space_id（首页教室列表点进来）：找到该教室则落到它。
        if (this.requestedSpace) {
          const s = spaces.find((x) => x.space_id === this.requestedSpace)
          if (s) {
            this.currentType = s.type
            this.current = s.space_id
          }
        }
        this.requestedSpace = ''
        const [doorRes] = await Promise.all([
          api.doorDevices(this.current === 'all' ? '' : this.current),
          this.fetchDevices(),
        ])
        this.doors = (doorRes && doorRes.doors) || []
        this.openWs()
        this.focusDevice()
      } catch (e) {
        uni.showToast({ title: e.message || '加载失败', icon: 'none' })
        uni.stopPullDownRefresh()
      }
    },
    onTypeChange(key) {
      if (key === this.currentType) return
      this.currentType = key
      const rooms = this.spaces.filter((s) => s.type === key)
      this.current = !this.isAdmin && rooms.length ? rooms[0].space_id : 'all'
      this.fetchDevices()
    },
    async switchSpace(key) {
      this.current = key
      const [doorRes] = await Promise.all([
        api.doorDevices(this.current === 'all' ? '' : this.current),
        this.fetchDevices(),
      ])
      this.doors = (doorRes && doorRes.doors) || []
      this.openWs()
    },
    onSpaceChange(e) {
      const idx = Number(e.detail.value)
      const s = this.spaceTabs[idx]
      if (s) this.switchSpace(s.key)
    },
    async fetchDevices() {
      try {
        const rooms =
          this.current === 'all'
            ? this.spaces.filter((s) => s.type === this.currentType)
            : this.spaces.filter((s) => s.space_id === this.current)
        const lists = await Promise.all(rooms.map((s) => api.devices(s.space_id)))
        let out = []
        rooms.forEach((s, i) => {
          ;(lists[i].devices || []).forEach((d) => out.push({ ...d, space_id: s.space_id }))
        })
        this.devices = out
      } catch (e) {
        uni.showToast({ title: e.message || '加载失败', icon: 'none' })
      } finally {
        uni.stopPullDownRefresh()
      }
    },
    focusDevice() {
      const id = this.requestedDevice
      this.requestedDevice = ''
      this.requestedFromAlert = ''
      if (!id) return
      const dev = this.devices.find((d) => d.device_id === id)
      if (!dev) {
        uni.showToast({ title: '关联设备已不在线或已移除', icon: 'none' })
        return
      }
      this.highlightDeviceId = id
      if (this.highlightTimer) clearTimeout(this.highlightTimer)
      this.highlightTimer = setTimeout(() => { this.highlightDeviceId = '' }, 3000)
      this.$nextTick(() => {
        uni.pageScrollTo({ selector: '#device-' + id, duration: 300 })
      })
    },
    openControl(d) {
      this.controlTarget = d
      this.sheetVisible = true
    },
    async toggleDoor(d) {
      const marked = this.isDoor(d)
      if (marked) {
        await api.unmarkDoor(d.space_id, d.device_id)
        this.doors = this.doors.filter((x) => !(x.space_id === d.space_id && x.device_id === d.device_id))
        uni.showToast({ title: '已取消门禁标记', icon: 'none' })
      } else {
        await api.markDoor({ device_id: d.device_id, space_id: d.space_id, label: d.label })
        this.doors.push({ space_id: d.space_id, device_id: d.device_id, label: d.label })
        uni.showToast({ title: '已标记为门禁', icon: 'none' })
      }
    },
    async sendCommand(c) {
      const d = this.controlTarget
      try {
        await this.doControl(d, c.cmd, false)
        this.sheetVisible = false
        // 200 仅表示「已下发 MQTT」，不等于设备已执行（见 api-tests/AUDIT P0-5），
        // 故用中性提示，真实执行结果由 cmd_response/轮询回填。
        uni.showToast({ title: '已发送，等待设备响应', icon: 'none' })
      } catch (e) {
        if (e && e.code === 'CONFIRM_REQUIRED') {
          // 开门（门禁 off）需二次确认（428）
          uni.showModal({
            title: '确认开门',
            content: '即将对「' + d.label + '」执行开门，请确认',
            confirmText: '确认开门',
            success: async (res) => {
              if (!res.confirm) return
              try {
                await this.doControl(d, c.cmd, true)
                this.sheetVisible = false
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
    openWs() {
      // admin 选「全部」时不指定 space_id（连全量快照）；teacher 连当前绑定教室。
      const spaceId = this.current === 'all' ? '' : this.current
      connectWs(spaceId, { onMessage: (msg) => this.applyWsMessage(msg) })
    },
    applyWsMessage(msg) {
      if (!msg) return
      if (msg.type === 'device_update') {
        const idx = this.devices.findIndex((d) => d.device_id === msg.device_id)
        if (idx >= 0) {
          this.devices[idx] = { ...this.devices[idx], status: msg.status, app: msg.app }
        }
      } else if (msg.type === 'cmd_response') {
        uni.showToast({ title: '设备已响应', icon: 'none' })
      }
    },
  },
}
</script>

<style scoped>
.devices {
  /* 注意：bottom 交给 .page-pad 让位给 tabbar，这里不要用 padding shorthand */
  padding-top: 24rpx;
  padding-left: 24rpx;
  padding-right: 24rpx;
}

/* 空间类型（第一级分组）：横向滚动，类型再多也放得下 */
.type-tabs {
  display: flex;
  margin-bottom: 24rpx;
  white-space: nowrap;
  overflow-x: auto;
}
.type-tab {
  flex: 0 0 auto;
  height: 64rpx;
  padding: 0 30rpx;
  margin-right: 16rpx;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 26rpx;
  color: var(--text-2);
  background: #fff;
  border: 1rpx solid var(--border-light);
  border-radius: 8rpx;
}
.type-tab:last-child {
  margin-right: 0;
}
.type-tab.active {
  color: var(--primary);
  background: var(--primary-soft);
  border-color: var(--primary);
}

.space-select {
  display: flex;
  align-items: center;
  justify-content: space-between;
  height: 80rpx;
  padding: 0 24rpx;
}
.space-select-label {
  font-size: 28rpx;
  color: var(--text-1);
}
.space-select-arrow {
  display: flex;
  align-items: center;
}

.search {
  display: flex;
  align-items: center;
  margin-top: 24rpx;
  padding: 0 24rpx;
  height: 80rpx;
}
.search-ico {
  display: flex;
  align-items: center;
  margin-right: 12rpx;
}
.search-input {
  flex: 1;
  height: 80rpx;
  font-size: 28rpx;
  color: var(--text-1);
}
.ph {
  color: var(--text-4);
}
.search-clear {
  display: flex;
  align-items: center;
  padding: 8rpx;
}

.summary {
  margin: 24rpx 4rpx 4rpx;
  font-size: 24rpx;
  color: var(--text-3);
  display: flex;
  align-items: center;
  flex-wrap: wrap;
}
.summary .sep {
  margin: 0 10rpx;
  color: var(--border);
}
.summary .n {
  font-weight: 600;
}
.summary .ok,
.summary .off {
  display: flex;
  align-items: center;
}
.summary .dot {
  width: 12rpx;
  height: 12rpx;
  border-radius: 50%;
  margin-right: 8rpx;
}
.summary .dot-online {
  background: var(--success);
}
.summary .dot-offline {
  background: var(--danger);
}

.list-group {
  margin-top: 12rpx;
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
.ctrl-btn {
  flex-shrink: 0;
  height: 60rpx;
  padding: 0 28rpx;
  margin: 0 0 0 16rpx;
  font-size: 26rpx;
  line-height: 60rpx;
  color: var(--primary);
  background: var(--primary-soft);
  border: 1rpx solid var(--primary-border);
  border-radius: 8rpx;
}
.ctrl-btn::after {
  border: none;
}
.ctrl-btn[disabled] {
  color: var(--text-4);
  background: var(--bg);
  border-color: var(--border-lighter);
}
.device-highlight {
  background: var(--primary-soft);
  transition: background 0.3s;
}

/* 控制弹层 */
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
  padding: 32rpx 32rpx calc(32rpx + env(safe-area-inset-bottom));
  transform: translateY(100%);
  transition: transform 0.25s ease;
}
.sheet.show {
  transform: translateY(0);
}
.sheet-head {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  margin-bottom: 24rpx;
}
.sheet-title {
  display: block;
  font-size: 34rpx;
  font-weight: 600;
  color: var(--text-1);
}
.sheet-sub {
  display: block;
  margin-top: 6rpx;
  font-size: 24rpx;
  color: var(--text-3);
}
.sheet-close {
  display: flex;
  align-items: center;
  padding: 8rpx;
}
.cmd-grid {
  display: flex;
  flex-wrap: wrap;
}
.cmd {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  width: calc(33.333% - 14rpx);
  height: 120rpx;
  margin: 0 20rpx 20rpx 0;
  background: var(--bg-soft);
  border: 1rpx solid var(--border-light);
  border-radius: 8rpx;
}
.cmd:nth-child(3n) {
  margin-right: 0;
}
.cmd.risk {
  background: var(--warning-bg);
  border-color: var(--warning-border);
}
.cmd-label {
  font-size: 28rpx;
  color: var(--text-1);
}
.cmd.risk .cmd-label {
  color: var(--warning);
}
.door-toggle {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 20rpx 4rpx 0;
  margin-top: 8rpx;
  border-top: 1rpx solid var(--border-lighter);
}
.door-toggle-label {
  font-size: 24rpx;
  color: var(--text-2);
}
.door-switch {
  width: 80rpx;
  height: 44rpx;
  border-radius: 22rpx;
  background: var(--border);
  position: relative;
  transition: background 0.2s;
}
.door-switch.on {
  background: var(--primary);
}
.door-knob {
  width: 36rpx;
  height: 36rpx;
  border-radius: 50%;
  background: #fff;
  position: absolute;
  top: 4rpx;
  left: 4rpx;
  transition: left 0.2s;
}
.door-switch.on .door-knob {
  left: 40rpx;
}
.cmd-risk {
  margin-top: 6rpx;
  font-size: 20rpx;
  color: var(--warning);
}
</style>
