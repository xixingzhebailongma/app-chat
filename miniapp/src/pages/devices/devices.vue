<template>
  <view class="devices page-pad">
    <!-- 空间类型（第一级） -->
    <scroll-view scroll-x class="type-bar" :show-scrollbar="false">
      <view
        v-for="t in typeTabs"
        :key="t.key"
        class="type-pill"
        :class="{ active: currentType === t.key }"
        @tap="onTypeChange(t.key)"
      >{{ t.label }}</view>
    </scroll-view>

    <!-- 空间选择（第二级） -->
    <picker
      mode="selector"
      :range="spaceTabs"
      range-key="label"
      @change="onSpaceChange"
    >
      <view class="card space-select">
        <text class="space-select-label">{{ currentSpaceLabel }}</text>
        <uni-icons type="arrow-down" size="16" color="var(--text-3)" />
      </view>
    </picker>

    <!-- 搜索 -->
    <view class="card search">
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
      <text>共 <text class="n">{{ visibleDevices.length }}</text> 台</text>
      <text class="sep">·</text>
      <text>在线 <text class="n ok">{{ onlineCount }}</text></text>
      <text class="sep">·</text>
      <text>离线 <text class="n off">{{ offlineCount }}</text></text>
    </view>

    <!-- 按教室分区：全局视图下每间教室一个区块并带头（可钻取）；单教室视图仅一组、无头 -->
    <view v-for="g in roomGroups" :key="g.space_id" class="room-block">
      <view v-if="isGrouped" class="card room-head" hover-class="cell-hover" @tap="switchSpace(g.space_id)">
        <view class="room-icon"><uni-icons type="home" size="18" color="var(--brand)" /></view>
        <view class="room-head-main">
          <text class="room-head-name">{{ g.name }}</text>
          <text class="room-head-sub">{{ g.online }}/{{ g.total }} 台在线</text>
        </view>
        <text class="room-head-btn">进入教室</text>
        <uni-icons type="arrow-right" size="14" color="var(--text-4)" />
      </view>

      <!-- 开关设备（对齐 smart device-row：图标方块 + 名称/副信息 + 开关） -->
      <view v-if="g.switches.length" class="card dev-card">
        <view class="card__title">开关设备<text class="card__more">{{ g.switches.length }} 台</text></view>
        <view
          v-for="d in g.switches"
          :key="d.device_id"
          class="dev-row"
          :class="{ 'dev-row--off': d.status !== 'online' }"
        >
          <view class="dev-icon" :style="d.status === 'online' ? { background: iconSoft(d) } : {}">
            <uni-icons
              :type="deviceTypeIcon(d.device_type, d.zb_type)"
              size="20"
              :color="d.status === 'online' ? iconColor(d) : 'var(--text-4)'"
            />
          </view>
          <view class="dev-main">
            <view class="dev-title">
              <text class="dev-label">{{ d.label }}</text>
              <view class="dev-badge" :class="d.status === 'online' ? 'is-online' : 'is-offline'">
                <view class="dev-badge-dot" />
                <text>{{ d.status === 'online' ? '在线' : '离线' }}</text>
              </view>
            </view>
            <text class="dev-sub">{{ devSub(d) }}</text>
          </view>
          <view class="dev-act">
            <!-- 门禁开门要二次确认，保留「控制」弹层；其余设备直接滑动开关 -->
            <button
              v-if="isDoor(d) || d.zb_role === 'door'"
              class="ctrl-btn"
              :disabled="d.status !== 'online'"
              @tap="openControl(d)"
            >控制</button>
            <switch
              v-else
              class="dev-switch"
              :checked="d.app === 'on'"
              :disabled="d.status !== 'online'"
              color="#245e70"
              @change="onToggle(d, $event)"
            />
          </view>
        </view>
      </view>

      <!-- 环境面板（fuhe 类） -->
      <view v-if="g.panels.length" class="card dev-card">
        <view class="card__title">环境面板<text class="card__more">{{ g.panels.length }} 台</text></view>
        <view
          v-for="d in g.panels"
          :key="d.device_id"
          class="dev-row"
          :class="{ 'dev-row--off': d.status !== 'online' }"
        >
          <view class="dev-icon" :style="d.status === 'online' ? { background: iconSoft(d) } : {}">
            <uni-icons
              :type="deviceTypeIcon(d.device_type, d.zb_type)"
              size="20"
              :color="d.status === 'online' ? iconColor(d) : 'var(--text-4)'"
            />
          </view>
          <view class="dev-main">
            <view class="dev-title">
              <text class="dev-label">{{ d.label }}</text>
              <view class="dev-badge" :class="d.status === 'online' ? 'is-online' : 'is-offline'">
                <view class="dev-badge-dot" />
                <text>{{ d.status === 'online' ? '在线' : '离线' }}</text>
              </view>
            </view>
            <text class="dev-sub">{{ devSub(d) }}</text>
          </view>
          <view class="dev-act">
            <!-- 门禁开门要二次确认，保留「控制」弹层；其余设备直接滑动开关 -->
            <button
              v-if="isDoor(d) || d.zb_role === 'door'"
              class="ctrl-btn"
              :disabled="d.status !== 'online'"
              @tap="openControl(d)"
            >控制</button>
            <switch
              v-else
              class="dev-switch"
              :checked="d.app === 'on'"
              :disabled="d.status !== 'online'"
              color="#245e70"
              @change="onToggle(d, $event)"
            />
          </view>
        </view>
      </view>

      <!-- 传感器（只读 · 对齐 smart env-grid 网格卡） -->
      <view v-if="g.sensors.length" class="card dev-card">
        <view class="card__title">传感器<text class="card__more">只读 · 实时上报</text></view>
        <view v-if="g.readings.length" class="env-grid">
          <view
            v-for="r in g.readings"
            :key="r.animKey"
            class="env-card"
            :class="'env-card--' + r.key"
            hover-class="env-card--press"
            :hover-stay-time="60"
          >
            <view v-if="r.alarm" class="env-warn">
              <uni-icons type="info-filled" size="16" color="var(--danger)" />
            </view>
            <view class="env-head">
              <view class="env-icon" :style="{ background: envTheme(r.key).soft }">
                <uni-icons :type="envTheme(r.key).icon" size="18" :color="envTheme(r.key).color" />
              </view>
              <text class="env-label">{{ r.label }}</text>
            </view>
            <view class="env-value">
              <text class="env-num num" :class="{ 'env-num--alarm': r.alarm }">{{ envAnim[r.animKey] || r.display }}</text>
              <text v-if="r.unit" class="env-unit">{{ r.unit }}</text>
            </view>
          </view>
        </view>
        <view v-else class="dev-row-empty">该设备未上报环境数据</view>
      </view>
    </view>

    <EmptyState
      v-if="!roomGroups.length"
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
import { deviceTypeLabel, spaceTypeLabel, formatTime } from '../../utils/format'
import { controlErrorMessage } from '../../utils/controlError'
import { connectWs, closeWs } from '../../utils/ws'
import { deviceTypeIcon } from '../../utils/icon'
import { parseSensorApp } from '../../utils/env'
import { dampValue, durationOf } from '../../utils/envDamp'
import EmptyState from '../../components/EmptyState.vue'
import AppTabBar from '../../components/AppTabBar.vue'

// 底部 tab 切换走的是 uni.reLaunch，页面会被整个销毁重建，data 全部重置。
// 不把「上次看的空间类型 + 教室」落到 storage，切回来就退回默认教室。
const VIEW_STATE_KEY = 'devices_view_state'

export default {
  components: { EmptyState, AppTabBar },
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
      devices: [],
      doors: [],
      keyword: '',
      sheetVisible: false,
      controlTarget: null,
      /* P1 差值阻尼：envAnim[key] = 当前正在显示的数值文案 */
      envAnim: {},
    }
  },
  computed: {
    isAdmin() {
      return store.role === 'admin'
    },
    typeTabs() {
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
    // 下拉里直接列全部教室（原来只列当前类型下的教室，管理员的教室一多就得来回切类型找）
    spaceTabs() {
      const tabs = []
      if (this.isAdmin) tabs.push({ key: 'all', label: '全部' + this.currentTypeLabel, type: '' })
      this.spaces
        .slice()
        .sort((a, b) => String(a.type).localeCompare(String(b.type)) || String(a.name).localeCompare(String(b.name)))
        .forEach((s) => {
          tabs.push({ key: s.space_id, type: s.type, label: s.name + ' · ' + spaceTypeLabel(s.type) })
        })
      return tabs
    },
    currentSpaceLabel() {
      const s = this.spaceTabs.find((x) => x.key === this.current)
      return s ? s.label : '请选择空间'
    },
    visibleDevices() {
      const k = this.keyword.trim().toLowerCase()
      if (!k) return this.devices
      return this.devices.filter(
        (d) => d.label.toLowerCase().includes(k) || d.device_id.toLowerCase().includes(k)
      )
    },
    onlineCount() {
      return this.visibleDevices.filter((d) => d.status === 'online').length
    },
    offlineCount() {
      return this.visibleDevices.length - this.onlineCount
    },
    // 管理员全局视图（current === 'all'）时多教室设备混排，分不清归属。
    // 统一按教室分组渲染：单教室视图下也只有一组，只是不显示组头。
    isGrouped() {
      return this.current === 'all'
    },
    roomGroups() {
      const order = []
      const byId = {}
      this.visibleDevices.forEach((d) => {
        let g = byId[d.space_id]
        if (!g) {
          g = { space_id: d.space_id, all: [], switches: [], panels: [], sensors: [] }
          byId[d.space_id] = g
          order.push(d.space_id)
        }
        g.all.push(d)
        // 分类条件与原三段卡片完全一致（fuhe 设备同时出现在开关/面板两段是现存行为，保留）
        if (this.isSwitchable(d)) g.switches.push(d)
        if (String(d.device_type).startsWith('fuhe')) g.panels.push(d)
        if (!this.isSwitchable(d) && !String(d.device_type).startsWith('fuhe')) g.sensors.push(d)
      })
      return order.map((sid) => {
        const g = byId[sid]
        const s = this.spaces.find((x) => x.space_id === sid)
        return {
          ...g,
          name: s ? s.name : sid,
          type: s ? s.type : '',
          total: g.all.length,
          online: g.all.filter((d) => d.status === 'online').length,
          // 读数按教室隔离：animKey 带 space_id，避免多教室同名温度互相串动画
          readings: this.buildReadings(g.sensors, sid),
        }
      })
    },
    // 阻尼/签名需要跨所有教室的读数扁平列表
    allReadings() {
      return this.roomGroups.reduce((arr, g) => arr.concat(g.readings), [])
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
      ]
    },
    /* 读数原始值签名：只用于驱动阻尼动画，不参与渲染。
     * animKey 含 space_id：多教室同指标（两间教室的温度）不会互相触发/串值 */
    envRawSig() {
      return this.allReadings.map((r) => r.animKey + '=' + r.raw).join('|')
    },
  },
  watch: {
    /* 同步跑：第一帧把 envAnim 写回旧值，抢在渲染之前落地，避免新值先闪一帧 */
    envRawSig() {
      this.runEnvDamp()
    },
  },
  created() {
    this._dampers = {}
    this._pageHidden = false
  },
  onLoad(options) {
    this.requestedType = (options && options.type) || ''
    this.requestedSpace = (options && options.space_id) || ''
    this.requestedDevice = (options && options.device_id) || ''
  },
  onShow() {
    if (!isLoggedIn()) {
      uni.reLaunch({ url: '/pages/login/login' })
      return
    }
    this._pageHidden = false
    /* 回到前台先补一拍：页隐藏期间读数变更被丢弃了，这里从上次显示值接上 */
    this.runEnvDamp()
    this.load()
  },
  onHide() {
    /* 页面不可见：立刻终止阻尼时间线，避免后台空转 setData */
    this._pageHidden = true
    this.cancelEnvDamp()
    closeWs()
  },
  onUnload() {
    this._pageHidden = true
    this.cancelEnvDamp()
    closeWs()
  },
  onPullDownRefresh() {
    this.fetchDevices()
  },
  methods: {
    deviceType: deviceTypeLabel,
    deviceTypeIcon,
    /* 设备图标色 / 浅底（统一蓝色体系，不再按设备类型分色） */
    iconColor(d) {
      return 'var(--brand)'
    },
    iconSoft(d) {
      return 'var(--brand-soft)'
    },
    devSub(d) {
      // 在线/离线已经由标题旁的状态徽标表达，这里只留上报时间，不再重复写「设备已离线」
      const timeText = d.last_seen ? '上报 ' + formatTime(d.last_seen) : ''
      if (d.status === 'online') {
        return timeText || this.deviceType(d.device_type, d.zb_type)
      }
      return timeText ? '最后' + timeText : '无上报记录'
    },
    /* 环境指标主题（图标按指标区分，配色统一蓝色） */
    envTheme(k) {
      const icons = {
        temperature: 'calendar',
        humidity: 'cloud-upload',
        co2: 'flag',
        light: 'sunny',
        pm25: 'info',
        pir: 'staff',
      }
      return { color: 'var(--brand)', soft: 'var(--brand-soft)', icon: icons[k] || icons.temperature }
    },
    envAlarm(k, v) {
      const n = Number(v)
      if (Number.isNaN(n)) return false
      if (k === 'temperature') return n >= 30
      if (k === 'humidity') return n < 30 || n > 70
      if (k === 'co2') return n >= 1500
      if (k === 'pm25') return n >= 75
      return false
    },
    /* 构建一间教室的传感器读数（只渲染 app JSON 里实际存在的字段）。
     * key 仍是指标名（用于样式/主题），animKey = 空间 + 指标（用于阻尼与渲染取值） */
    buildReadings(sensorDevices, spaceId) {
      const meta = {
        temperature: { label: '温度', unit: '℃', decimals: 1 },
        humidity: { label: '湿度', unit: '%RH', decimals: 0 },
        co2: { label: 'CO₂', unit: 'ppm', decimals: 0 },
        light: { label: '光照', unit: 'lux', decimals: 0 },
        pm25: { label: 'PM2.5', unit: 'μg/m³', decimals: 0 },
        pir: { label: '人体感应', unit: '', boolean: true },
      }
      const merged = {}
      sensorDevices.forEach((d) => {
        const o = parseSensorApp(d)
        if (!o) return
        Object.keys(o).forEach((k) => { if (meta[k] && o[k] != null) merged[k] = o[k] })
      })
      return Object.keys(meta)
        .filter((k) => k in merged)
        .map((k) => {
          const m = meta[k]
          const v = merged[k]
          return {
            key: k,
            animKey: spaceId + '__' + k,
            label: m.label,
            unit: m.unit,
            /* 原始数值与小数位：只给差值阻尼用（P1） */
            raw: v,
            decimals: m.decimals || 0,
            isBool: !!m.boolean,
            display: m.boolean ? (v ? '有人' : '无人') : Number(v).toFixed(m.decimals || 0),
            alarm: this.envAlarm(k, v),
          }
        })
    },
    isSwitchable(d) {
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
        const saved = this.readViewState()
        let type = this.typeTabs.length ? this.typeTabs[0].key : ''
        if (saved.currentType && this.typeTabs.some((t) => t.key === saved.currentType)) {
          type = saved.currentType
        }
        if (this.requestedType && this.typeTabs.some((t) => t.key === this.requestedType)) {
          type = this.requestedType
        }
        this.currentType = type
        this.requestedType = ''
        this.current = !this.isAdmin && spaces.length ? spaces[0].space_id : 'all'
        // 优先恢复上次查看的教室；带 space_id 跳进来的（从「我的教室」点某个教室）优先级最高
        const savedSpace = saved.current
        const savedValid =
          savedSpace === 'all'
            ? this.isAdmin
            : spaces.some((s) => s.space_id === savedSpace)
        if (savedValid) this.current = savedSpace
        if (this.requestedSpace) {
          const s = spaces.find((x) => x.space_id === this.requestedSpace)
          if (s) {
            this.currentType = s.type
            this.current = s.space_id
          }
        }
        this.requestedSpace = ''
        this.saveViewState()
        const [doorRes] = await Promise.all([
          api.doorDevices(this.current === 'all' ? '' : this.current),
          this.fetchDevices(),
        ])
        this.doors = (doorRes && doorRes.doors) || []
        this.openWs()
      } catch (e) {
        uni.showToast({ title: e.message || '加载失败', icon: 'none' })
        uni.stopPullDownRefresh()
      }
    },
    /* 记住当前查看的空间类型 + 教室，供下次进入（tab 切换会重建页面）恢复 */
    readViewState() {
      try {
        const raw = uni.getStorageSync(VIEW_STATE_KEY)
        return raw && typeof raw === 'object' ? raw : {}
      } catch (e) {
        return {}
      }
    },
    saveViewState() {
      try {
        uni.setStorageSync(VIEW_STATE_KEY, {
          current: this.current,
          currentType: this.currentType,
        })
      } catch (e) {}
    },
    onTypeChange(key) {
      if (key === this.currentType) return
      this.currentType = key
      const rooms = this.spaces.filter((s) => s.type === key)
      this.current = !this.isAdmin && rooms.length ? rooms[0].space_id : 'all'
      this.saveViewState()
      this.fetchDevices()
    },
    async switchSpace(key) {
      this.current = key
      this.saveViewState()
      const [doorRes] = await Promise.all([
        api.doorDevices(this.current === 'all' ? '' : this.current),
        this.fetchDevices(),
      ])
      this.doors = (doorRes && doorRes.doors) || []
      this.openWs()
    },
    onSpaceChange(e) {
      const s = this.spaceTabs[Number(e.detail.value)]
      if (!s) return
      // 选了具体教室就把类型筛选同步到它的类型，避免上方 pills 和实际查看的教室错位
      if (s.type && s.type !== this.currentType) this.currentType = s.type
      this.switchSpace(s.key)
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
    openControl(d) {
      this.controlTarget = d
      this.sheetVisible = true
    },
    /* 滑动开关：先乐观更新滑块位置，下发成功保留，失败回滚。
       switch 的 checked 绑的是 d.app，所以回滚后滑块会自己弹回原位。 */
    async onToggle(d, e) {
      const on = !!(e && e.detail && e.detail.value)
      const idx = this.devices.findIndex(
        (x) => x.device_id === d.device_id && x.space_id === d.space_id
      )
      const prev = idx >= 0 ? this.devices[idx].app : ''
      if (idx >= 0) {
        this.devices[idx] = { ...this.devices[idx], app: on ? 'on' : 'off' }
      }
      try {
        await this.doControl(d, on ? 'on' : 'off', false)
        uni.showToast({ title: on ? '已开启' : '已关闭', icon: 'none' })
      } catch (err) {
        if (idx >= 0 && prev) {
          this.devices[idx] = { ...this.devices[idx], app: prev }
        }
        uni.showToast({ title: controlErrorMessage(err) || '下发失败', icon: 'none' })
      }
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
    /* ============================================================
     * P1 · 环境读数差值阻尼（对齐《晴昼 1.4.2 交付说明》§三）
     * 上次值在 utils/envDamp.js 的模块级 Map 里，与页面生命周期解耦：
     * 换空间 / 切 Tab 回来，仍是「从旧值滚到新值」而不是从 0 重滚。
     * ============================================================ */
    runEnvDamp() {
      if (this._pageHidden) return
      const alive = {}
      this.allReadings.forEach((r) => {
        alive[r.animKey] = true
        if (r.isBool) {
          this.setEnvAnim(r.animKey, r.display)
          return
        }
        this._dampers[r.animKey] = dampValue(r.animKey, r.raw, {
          duration: durationOf(r.key),
          onFrame: (val) => {
            this.setEnvAnim(r.animKey, Number(val).toFixed(r.decimals))
          },
        })
      })
      Object.keys(this._dampers).forEach((k) => {
        if (alive[k]) return
        this._dampers[k]()
        delete this._dampers[k]
        if (this.envAnim[k] !== undefined) {
          const next = { ...this.envAnim }
          delete next[k]
          this.envAnim = next
        }
      })
    },
    setEnvAnim(key, text) {
      if (this.envAnim[key] === text) return
      this.envAnim = { ...this.envAnim, [key]: text }
    },
    cancelEnvDamp() {
      Object.keys(this._dampers).forEach((k) => {
        this._dampers[k]()
        delete this._dampers[k]
      })
    },
    openWs() {
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
      }
    },
  },
}
</script>

<style scoped>
.devices {
  padding-top: 20rpx;
  /* 底部让位与全局 .page-pad 对齐：固定 TabBar（100rpx+安全区）+ 呼吸空间 */
  padding-bottom: calc(240rpx + env(safe-area-inset-bottom));
  /* 顶部一段淡蓝渐变，与教师端「我的教室」统一 */
  /* 晴昼：环境光双径向叠层（对齐 qingzhou body） */
  background:
    radial-gradient(ellipse at 30% -10%, var(--ambient-a), transparent 55%),
    radial-gradient(ellipse at 95% 70%, var(--ambient-b), transparent 52%),
    var(--bg-page);
}

/* 空间类型 */
.type-bar {
  white-space: nowrap;
  margin: 0 36rpx 28rpx;
}
.type-pill {
  display: inline-flex;
  align-items: center;
  height: 56rpx;
  padding: 0 26rpx;
  margin-right: 14rpx;
  background: #fff;
  border: 1rpx solid var(--border);
  border-radius: 999rpx;
  font-size: 28rpx;
  font-weight: 400;
  color: var(--text-2);
}
.type-pill.active {
  background: var(--primary);
  color: #fff;
  border-color: var(--primary);
}

/* 空间选择 */
.space-select {
  display: flex;
  align-items: center;
  justify-content: space-between;
  height: 80rpx;
  padding: 0 32rpx;
  margin: 0 36rpx 28rpx;
}
.space-select-label {
  font-size: 28rpx;
  font-weight: 400;
  color: var(--text-1);
}

/* 搜索 */
.search {
  display: flex;
  align-items: center;
  margin: 0 36rpx 28rpx;
  padding: 0 32rpx;
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
  font-weight: 400;
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

/* 汇总 */
.summary {
  margin: 0 40rpx 16rpx;
  font-size: 26rpx;
  font-weight: 400;
  color: var(--text-3);
  display: flex;
  align-items: center;
}
.summary .sep {
  margin: 0 10rpx;
  color: var(--border);
}
.summary .n {
  font-weight: 500;
}
.summary .ok {
  color: var(--success);
}
.summary .off {
  color: var(--danger);
}

/* 教室分区：全局视图下区块之间留一组间距 */
.room-block {
  margin-bottom: 12rpx;
}
.room-head {
  display: flex;
  align-items: center;
  gap: 16rpx;
  margin: 4rpx 36rpx 20rpx;
  padding: 22rpx 28rpx;
  background: linear-gradient(140deg, var(--bg-card), var(--bg-card-end));
  border: 1rpx solid var(--border-light);
}
.room-icon {
  width: 60rpx;
  height: 60rpx;
  border-radius: 16rpx;
  flex-shrink: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  background: var(--brand-soft);
}
.room-head-main {
  flex: 1;
  min-width: 0;
  display: flex;
  flex-direction: column;
  gap: 6rpx;
}
.room-head-name {
  font-size: 30rpx;
  font-weight: 600;
  color: var(--text-1);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.room-head-sub {
  font-size: 22rpx;
  color: var(--text-3);
}
.room-head-btn {
  flex-shrink: 0;
  padding: 8rpx 22rpx;
  font-size: 24rpx;
  color: var(--primary);
  background: var(--primary-soft);
  border: 1rpx solid var(--primary-border);
  border-radius: 999rpx;
}

/* 设备卡片（对齐 smart：$page-pad 36 / $card-gap 28 / $card-pad 32） */
.dev-card {
  margin: 0 36rpx 28rpx;
  padding: 32rpx;
}
.card__title {
  display: flex;
  align-items: center;
  justify-content: space-between;
  font-size: 32rpx;
  font-weight: 600;
  color: var(--text-1);
  margin-bottom: 20rpx;
  position: relative;
  padding-left: 22rpx;
}
.card__title::before {
  content: '';
  position: absolute;
  left: 0;
  top: 50%;
  transform: translateY(-50%);
  width: 8rpx;
  height: 30rpx;
  border-radius: 4rpx;
  background: var(--brand);
}
.card__more {
  font-size: 26rpx;
  font-weight: 400;
  color: var(--text-3);
}

/* 设备行（对齐 smart device-row：图标方块 + 名称/副信息 + 右侧操作，行高 ≥120rpx） */
.dev-row {
  display: flex;
  align-items: center;
  gap: 20rpx;
  min-height: 120rpx;
  padding: 20rpx 0;
  border-bottom: 1rpx solid var(--border-lighter);
  transition: opacity 200ms cubic-bezier(0.25, 0.1, 0.25, 1);
}
.dev-row:last-child {
  border-bottom: none;
}
.dev-row--off {
  /* 0.55 太淡，离线的状态徽标和上报时间会糊掉；0.72 既弱化又看得清 */
  opacity: 0.72;
}
.dev-row-empty {
  text-align: center;
  color: var(--text-3);
  font-size: 28rpx;
  padding: 30rpx 0 10rpx;
}
.dev-icon {
  width: 72rpx;
  height: 72rpx;
  border-radius: 18rpx;
  display: flex;
  align-items: center;
  justify-content: center;
  flex-shrink: 0;
  background: var(--bg-soft);
}
.dev-main {
  flex: 1;
  display: flex;
  flex-direction: column;
  gap: 8rpx;
  min-width: 0;
}
.dev-title {
  display: flex;
  align-items: center;
  gap: 12rpx;
  min-width: 0;
}
/* 在线状态徽标：绿=在线、灰=离线，不用读副信息就能分清 */
.dev-badge {
  flex-shrink: 0;
  display: flex;
  align-items: center;
  gap: 6rpx;
  height: 32rpx;
  padding: 0 12rpx;
  border-radius: 999rpx;
  font-size: 20rpx;
  line-height: 32rpx;
}
.dev-badge-dot {
  width: 8rpx;
  height: 8rpx;
  border-radius: 50%;
  background: currentColor;
}
.dev-badge.is-online {
  color: var(--success);
  background: var(--success-bg);
}
.dev-badge.is-offline {
  color: var(--text-3);
  background: var(--bg-soft);
}
.dev-label {
  font-size: 28rpx;
  font-weight: 500;
  color: var(--text-1);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
/* 标题行空间不够时优先截断设备名，别把状态徽标挤走 */
.dev-title .dev-label {
  flex: 1;
  min-width: 0;
}
.dev-sub {
  font-size: 26rpx;
  font-weight: 400;
  color: var(--text-3);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.dev-act {
  flex-shrink: 0;
}
/* 原生开关默认 52×32px，偏大，缩到与 120rpx 行高匹配（靠右对齐，不挤文字） */
.dev-switch {
  flex-shrink: 0;
  transform: scale(0.85);
  transform-origin: 100% 50%;
}
.ctrl-btn {
  flex-shrink: 0;
  height: 64rpx;
  padding: 0 32rpx;
  margin: 0;
  font-size: 26rpx;
  line-height: 64rpx;
  font-weight: 500;
  color: var(--primary);
  background: var(--primary-soft);
  border: 1rpx solid var(--primary-border);
  border-radius: 999rpx;
}
.ctrl-btn::after {
  border: none;
}
.ctrl-btn[disabled] {
  color: var(--text-4);
  background: var(--bg);
  border-color: var(--border-lighter);
}

/* 传感器网格卡（统一蓝色体系：白卡 + 蓝色顶条，指标区分靠图标与文字，不靠底色） */
.env-grid {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 16rpx;
}
.env-card {
  position: relative;
  background: #fff;
  border-radius: 24rpx;
  border: 1rpx solid var(--border);
  box-shadow: 0 6rpx 16rpx rgba(96, 81, 51, 0.05);
  padding: 24rpx;
  transition: transform 150ms cubic-bezier(0.25, 0.1, 0.25, 1);
  overflow: hidden;
}
.env-card::before {
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  height: 6rpx;
  background: var(--brand-grad);
}
.env-card--temperature,
.env-card--humidity,
.env-card--co2,
.env-card--pm25,
.env-card--light,
.env-card--pir {
  background: #fff;
  border-color: var(--border);
}
.env-card--temperature::before,
.env-card--humidity::before,
.env-card--co2::before,
.env-card--pm25::before,
.env-card--light::before,
.env-card--pir::before {
  background: var(--brand-grad);
}
.env-card--press {
  transform: scale(0.97);
}
.env-head {
  display: flex;
  align-items: center;
  gap: 12rpx;
  margin-bottom: 16rpx;
}
.env-icon {
  width: 56rpx;
  height: 56rpx;
  border-radius: 16rpx;
  display: flex;
  align-items: center;
  justify-content: center;
  flex-shrink: 0;
  background: var(--brand-soft);
}
.env-label {
  font-size: 26rpx;
  font-weight: 500;
  color: var(--text-2);
  overflow: hidden;
  white-space: nowrap;
  text-overflow: ellipsis;
}
.env-value {
  display: flex;
  align-items: flex-end;
  gap: 8rpx;
}
.env-num {
  min-width: 128rpx;
  font-size: 56rpx;
  font-weight: 700;
  line-height: 1.1;
  color: var(--text-1);
}
.env-unit {
  font-size: 24rpx;
  font-weight: 400;
  color: var(--text-3);
  line-height: 1.5;
}
.env-num--alarm {
  color: var(--danger);
}
.env-warn {
  position: absolute;
  top: 16rpx;
  right: 16rpx;
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
  border-radius: 24rpx 24rpx 0 0;
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
  font-size: 36rpx;
  font-weight: 600;
  color: var(--text-1);
}
.sheet-sub {
  display: block;
  margin-top: 6rpx;
  font-size: 26rpx;
  font-weight: 400;
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
  border-radius: 16rpx;
}
.cmd:nth-child(3n) {
  margin-right: 0;
}
.cmd-label {
  font-size: 28rpx;
  font-weight: 400;
  color: var(--text-1);
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
  font-size: 28rpx;
  font-weight: 400;
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
</style>
