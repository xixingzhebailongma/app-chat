<template>
  <view class="alerts page-pad">
    <!-- 状态筛选 -->
    <view class="filter-bar">
      <view
        v-for="f in statusFilters"
        :key="f.key"
        class="filter-pill"
        :class="{ active: statusFilter === f.key }"
        @tap="setStatus(f.key)"
      >
        {{ f.label }}
      </view>
    </view>

    <!-- 类型筛选 -->
    <scroll-view scroll-x class="type-bar" :show-scrollbar="false">
      <view
        v-for="t in typeFilters"
        :key="t.key"
        class="type-pill"
        :class="{ active: typeFilter === t.key }"
        @tap="setType(t.key)"
      >
        {{ t.label }}
      </view>
    </scroll-view>

    <!-- 教室 + 时间筛选 -->
    <view class="filters">
      <picker
        mode="selector"
        :range="spaceTabs"
        range-key="label"
        @change="onSpaceChange"
      >
        <view class="filter card">
          <text class="filter-label">{{ currentSpaceLabel }}</text>
          <uni-icons type="arrow-down" size="16" color="var(--text-3)" />
        </view>
      </picker>
      <picker
        mode="selector"
        :range="timeTabs"
        range-key="label"
        @change="onTimeChange"
      >
        <view class="filter card">
          <text class="filter-label">{{ currentTimeLabel }}</text>
          <uni-icons type="arrow-down" size="16" color="var(--text-3)" />
        </view>
      </picker>
    </view>

    <!-- 汇总 -->
    <view class="summary">
      共 <text class="n">{{ total }}</text> 条告警
      <text class="hint">{{ isAdmin ? ' · 点「处理」标记处理中或已处理' : ' · 由管理员统一处理' }}</text>
    </view>

    <!-- 统计卡（全量概览，不随列表筛选变化） -->
    <view v-if="stats" class="stat-row">
      <view v-for="s in statusStats" :key="s.key" class="stat-card card">
        <view class="stat-num">
          <view v-if="s.accent" class="stat-dot"></view>
          <text class="stat-n">{{ s.count }}</text>
        </view>
        <text class="stat-l">{{ s.label }}</text>
      </view>
    </view>
    <view v-if="stats && typeStats.length" class="type-line">
      <text v-for="t in typeStats" :key="t.key" class="type-item">{{ t.label }} {{ t.count }}</text>
    </view>
    <view v-if="isAdmin && spaceStats.length" class="space-line">
      <text class="space-label">教室分布</text>
      <text v-for="s in spaceStats" :key="s.space_id" class="space-item">{{ spaceName(s.space_id) }} {{ s.count }}</text>
    </view>
    <view v-if="unboundTeacher" class="unbound-hint">当前账号未绑定教室，请联系管理员分配</view>

    <!-- 告警列表 -->
    <view v-if="alerts.length">
      <view class="list-group">
        <view v-for="a in alerts" :key="a.alert_id" class="list-cell" hover-class="cell-hover" @tap="goDetail(a)">
          <view class="a-icon"><uni-icons :type="eventTypeIcon(a.event_type)" size="20" color="var(--text-2)" /></view>
          <view class="a-main">
            <view class="a-title">
              <text class="a-label">{{ eventLabel(a.event_type) }}</text>
              <StatusTag :text="statusLabel(a.status)" :tone="statusTone(a.status)" />
            </view>
            <text class="a-sub">{{ spaceName(a.space_id) }}<text v-if="a.device_label"> · {{ a.device_label }}</text><text v-if="a.created_at"> · {{ fmt(a.created_at) }}</text></text>
            <text v-if="isAdmin && (a.operator_name || a.remark)" class="a-remark">
              <text v-if="a.operator_name">操作人 {{ a.operator_name }}</text>
              <text v-if="a.operator_name && a.remark"> · </text>
              <text v-if="a.remark">处置：{{ a.remark }}</text>
            </text>
          </view>
          <button
            v-if="isAdmin && a.status !== 'resolved'"
            class="handle-btn"
            @tap.stop="openHandle(a)"
          >
            处理
          </button>
        </view>
      </view>
      <view v-if="hasMore" class="load-more" @tap="loadMore">加载更多</view>
      <view v-else class="load-end">没有更多了</view>
    </view>
    <EmptyState v-else title="没有符合条件的告警" desc="当前筛选下一切正常" icon="checkbox-filled" />

    <!-- 处理弹层 -->
    <view v-if="handleVisible" class="mask" @tap="handleVisible = false"></view>
    <view class="sheet" :class="{ show: handleVisible }">
      <view class="sheet-head">
        <text class="sheet-title">处理告警</text>
        <view class="sheet-close" @tap="handleVisible = false"><uni-icons type="close" size="18" color="var(--text-4)" /></view>
      </view>
      <text v-if="handleTarget" class="sheet-sub">
        {{ eventLabel(handleTarget.event_type) }} · {{ spaceName(handleTarget.space_id) }}
      </text>
      <textarea
        v-model="remark"
        class="remark"
        placeholder="处置说明（可选，如：已到现场检查，设备已恢复）"
        placeholder-class="ph"
        :maxlength="200"
      />
      <view class="sheet-actions">
        <button
          v-if="handleTarget && handleTarget.status === 'unhandled'"
          class="btn btn-ghost"
          :disabled="submitting"
          @tap="confirmHandle('handling')"
        >设为处理中</button>
        <button
          class="btn btn-primary"
          :disabled="submitting"
          @tap="confirmHandle('resolved')"
        >设为已处理</button>
      </view>
    </view>

    <AppTabBar current="alerts" />
  </view>
</template>

<script>
import { api } from '../../api/index'
import { store, isLoggedIn } from '../../store/index'
import {
  eventTypeLabel,
  alertStatusLabel,
  alertStatusTone,
  formatTime,
} from '../../utils/format'
import { eventTypeIcon } from '../../utils/icon'
import { alertErrorMessage } from '../../utils/alertError'
import StatusTag from '../../components/StatusTag.vue'
import EmptyState from '../../components/EmptyState.vue'
import AppTabBar from '../../components/AppTabBar.vue'

export default {
  components: { StatusTag, EmptyState, AppTabBar },
  data() {
    return {
      store,
      spaces: [],
      alerts: [],
      stats: null,
      total: 0,
      page: 1,
      pageSize: 20,
      hasMore: false,
      loading: false,
      statusFilter: 'all',
      typeFilter: 'all',
      spaceFilter: '',
      timeFilter: '',
      handleVisible: false,
      handleTarget: null,
      remark: '',
      submitting: false,
      statusFilters: [
        { key: 'all', label: '全部' },
        { key: 'unhandled', label: '待处理' },
        { key: 'handling', label: '处理中' },
        { key: 'resolved', label: '已处理' },
      ],
      typeFilters: [
        { key: 'all', label: '全部类型' },
        { key: 'device_offline', label: '设备离线' },
        { key: 'sensor_threshold', label: '传感器超阈值' },
        { key: 'face_login_failed', label: '人脸识别失败' },
      ],
      timeTabs: [
        { key: '', label: '全部时间' },
        { key: 'today', label: '今天' },
        { key: '7d', label: '近 7 天' },
        { key: '30d', label: '近 30 天' },
      ],
    }
  },
  computed: {
    isAdmin() {
      return store.role === 'admin'
    },
    spaceTabs() {
      const tabs = []
      if (this.isAdmin) tabs.push({ key: '', label: '全部教室' })
      else tabs.push({ key: '', label: '全部绑定教室' })
      this.spaces.forEach((s) => tabs.push({ key: s.space_id, label: s.name }))
      return tabs
    },
    currentSpaceLabel() {
      const s = this.spaceTabs.find((x) => x.key === this.spaceFilter)
      return s ? s.label : '全部'
    },
    currentTimeLabel() {
      const t = this.timeTabs.find((x) => x.key === this.timeFilter)
      return t ? t.label : '全部时间'
    },
    statusStats() {
      const s = (this.stats && this.stats.by_status) || {}
      return [
        { key: 'unhandled', label: '待处理', accent: true, count: s.unhandled || 0 },
        { key: 'handling', label: '处理中', count: s.handling || 0 },
        { key: 'resolved', label: '已处理', count: s.resolved || 0 },
      ]
    },
    typeStats() {
      const s = (this.stats && this.stats.by_event_type) || {}
      const order = ['device_offline', 'sensor_threshold', 'face_login_failed']
      const known = order
        .filter((k) => s[k] != null)
        .map((k) => ({ key: k, label: eventTypeLabel(k), count: s[k] }))
      const rest = Object.keys(s)
        .filter((k) => !order.includes(k))
        .map((k) => ({ key: k, label: eventTypeLabel(k), count: s[k] }))
      return known.concat(rest)
    },
    spaceStats() {
      const arr = (this.stats && this.stats.by_space) || []
      return arr.slice().sort((a, b) => b.count - a.count).slice(0, 5)
    },
    unboundTeacher() {
      return !this.isAdmin && this.spaces.length === 0
    },
  },
  onShow() {
    if (!isLoggedIn()) {
      uni.reLaunch({ url: '/pages/login/login' })
      return
    }
    this.load()
  },
  onPullDownRefresh() {
    this.fetchAlerts(true)
  },
  onReachBottom() {
    this.loadMore()
  },
  methods: {
    eventLabel: eventTypeLabel,
    eventTypeIcon,
    statusTone: alertStatusTone,
    fmt: formatTime,
    // 教师视角：把「处理中/已处理」说成「管理员处理中/管理员已处理」，让教师
    // 知道有人在管（P1 结论：教师不展示时间线/操作人/备注，但仍看到状态）。
    statusLabel(status) {
      if (!this.isAdmin && status === 'handling') return '管理员处理中'
      if (!this.isAdmin && status === 'resolved') return '管理员已处理'
      return alertStatusLabel(status)
    },
    spaceName(id) {
      const s = this.spaces.find((x) => x.space_id === id)
      return s ? s.name : id
    },
    setStatus(key) {
      this.statusFilter = key
      this.fetchAlerts(true)
    },
    setType(key) {
      this.typeFilter = key
      this.fetchAlerts(true)
    },
    onSpaceChange(e) {
      const idx = Number(e.detail.value)
      const s = this.spaceTabs[idx]
      if (s && s.key !== this.spaceFilter) {
        this.spaceFilter = s.key
        this.fetchAlerts(true)
      }
    },
    onTimeChange(e) {
      const idx = Number(e.detail.value)
      const t = this.timeTabs[idx]
      if (t && t.key !== this.timeFilter) {
        this.timeFilter = t.key
        this.fetchAlerts(true)
      }
    },
    timeRange() {
      if (this.timeFilter === 'today') {
        const d = new Date()
        const start = new Date(d.getFullYear(), d.getMonth(), d.getDate())
        return { from: start.toISOString(), to: '' }
      }
      if (this.timeFilter === '7d' || this.timeFilter === '30d') {
        const days = this.timeFilter === '7d' ? 7 : 30
        return { from: new Date(Date.now() - days * 24 * 3600 * 1000).toISOString(), to: '' }
      }
      return { from: '', to: '' }
    },
    buildQuery() {
      const q = { page: this.page, page_size: this.pageSize }
      if (this.spaceFilter) q.space_id = this.spaceFilter
      if (this.typeFilter !== 'all') q.event_type = this.typeFilter
      if (this.statusFilter !== 'all') q.status = this.statusFilter
      const range = this.timeRange()
      if (range.from) q.from = range.from
      if (range.to) q.to = range.to
      return q
    },
    async load() {
      try {
        const [spaces, stats] = await Promise.all([api.spaces(), api.alertStats()])
        this.spaces = spaces
        this.stats = stats || null
        await this.fetchAlerts(true)
      } catch (e) {
        uni.showToast({ title: alertErrorMessage(e) || '加载失败', icon: 'none' })
        uni.stopPullDownRefresh()
      }
    },
    async fetchAlerts(reset) {
      if (this.loading) return
      if (reset) {
        this.page = 1
        this.alerts = []
      }
      this.loading = true
      try {
        const res = await api.alerts(this.buildQuery())
        const rows = (res && res.alerts) || []
        this.alerts = reset ? rows : this.alerts.concat(rows)
        this.total = (res && res.total) || 0
        this.hasMore = !!(res && res.has_more)
        // 角标：待处理总数（独立轻量查询，服务端按角色收窄空间）。
        const unhandled = await api.alerts({ status: 'unhandled', page: 1, page_size: 1 })
        store.unhandledAlerts = (unhandled && unhandled.total) || 0
      } catch (e) {
        uni.showToast({ title: alertErrorMessage(e) || '加载失败', icon: 'none' })
      } finally {
        this.loading = false
        uni.stopPullDownRefresh()
      }
    },
    loadMore() {
      if (!this.hasMore || this.loading) return
      this.page += 1
      this.fetchAlerts(false)
    },
    openHandle(a) {
      this.handleTarget = a
      this.remark = ''
      this.submitting = false
      this.handleVisible = true
    },
    // 告警详情（admin 见处理过程；教师见脱敏摘要）。
    goDetail(a) {
      uni.navigateTo({ url: '/pages/alerts/detail?id=' + encodeURIComponent(a.alert_id) })
    },
    async confirmHandle(status) {
      if (this.submitting) return
      const id = this.handleTarget.alert_id
      this.submitting = true
      try {
        await api.handleAlert(id, this.remark.trim(), status)
        this.handleVisible = false
        uni.showToast({ title: status === 'resolved' ? '已处理' : '已标记为处理中', icon: 'success' })
        this.fetchAlerts(true)
      } catch (e) {
        uni.showToast({ title: alertErrorMessage(e) || '处理失败', icon: 'none' })
      } finally {
        this.submitting = false
      }
    },
  },
}
</script>

<style scoped>
.alerts {
  /* 注意：bottom 交给 .page-pad 让位给 tabbar，这里不要用 padding shorthand */
  padding-top: 24rpx;
  padding-left: 24rpx;
  padding-right: 24rpx;
}
.filter-bar {
  display: flex;
  gap: 16rpx;
}
.filter-pill {
  flex: 1;
  display: flex;
  align-items: center;
  justify-content: center;
  height: 64rpx;
  background: #fff;
  border: 1rpx solid var(--border);
  border-radius: 8rpx;
  font-size: 26rpx;
  color: var(--text-2);
}
.filter-pill.active {
  background: var(--bg-soft);
  color: var(--text-1);
  border-color: var(--primary);
}

.type-bar {
  white-space: nowrap;
  margin: 20rpx -24rpx 0;
  padding: 0 24rpx;
}
.type-pill {
  display: inline-flex;
  align-items: center;
  height: 56rpx;
  padding: 0 26rpx;
  margin-right: 14rpx;
  background: #fff;
  border: 1rpx solid var(--border);
  border-radius: 8rpx;
  font-size: 24rpx;
  color: var(--text-2);
}
.type-pill.active {
  background: var(--bg-soft);
  color: var(--text-1);
  border-color: var(--primary);
}

.filters {
  display: flex;
  gap: 20rpx;
  margin-top: 20rpx;
}
.filter {
  flex: 1;
  display: flex;
  align-items: center;
  justify-content: space-between;
  height: 80rpx;
  padding: 0 24rpx;
}
.filter-label {
  font-size: 28rpx;
  color: var(--text-1);
}

.summary {
  margin: 22rpx 4rpx 4rpx;
  font-size: 24rpx;
  color: var(--text-3);
}
.summary .n {
  font-weight: 600;
}
.summary .hint {
  color: var(--text-4);
}
.stat-row {
  display: flex;
  gap: 16rpx;
  margin-top: 20rpx;
}
.stat-card {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  padding: 20rpx 0;
}
.stat-num {
  display: flex;
  align-items: center;
}
.stat-n {
  font-size: 40rpx;
  font-weight: 700;
  line-height: 1;
  color: var(--text-1);
}
.stat-dot {
  width: 12rpx;
  height: 12rpx;
  border-radius: 50%;
  margin-right: 10rpx;
  background: var(--warning);
}
.stat-l {
  margin-top: 8rpx;
  font-size: 22rpx;
  color: var(--text-3);
}
.type-line {
  display: flex;
  flex-wrap: wrap;
  gap: 12rpx;
  margin-top: 16rpx;
  padding: 0 4rpx;
}
.type-item {
  font-size: 22rpx;
  color: var(--text-2);
}
.space-line {
  display: flex;
  flex-wrap: wrap;
  gap: 12rpx;
  margin-top: 12rpx;
  padding: 0 4rpx;
}
.space-label {
  font-size: 22rpx;
  color: var(--text-4);
}
.space-item {
  font-size: 22rpx;
  color: var(--text-2);
}
.unbound-hint {
  margin-top: 16rpx;
  padding: 16rpx 20rpx;
  font-size: 24rpx;
  color: var(--warning);
  background: var(--warning-bg);
  border: 1rpx solid var(--warning-border);
  border-radius: 8rpx;
}

.list-group {
  margin-top: 12rpx;
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
.a-title {
  display: flex;
  align-items: center;
}
.a-label {
  font-size: 30rpx;
  font-weight: 600;
  color: var(--text-1);
  margin-right: 14rpx;
}
.a-sub {
  margin-top: 6rpx;
  font-size: 22rpx;
  color: var(--text-3);
}
.a-remark {
  margin-top: 6rpx;
  font-size: 22rpx;
  color: var(--text-3);
}
.handle-btn {
  flex-shrink: 0;
  align-self: center;
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
.handle-btn::after {
  border: none;
}

.load-more {
  padding: 24rpx 0;
  text-align: center;
  font-size: 26rpx;
  color: var(--primary);
}
.load-end {
  padding: 24rpx 0;
  text-align: center;
  font-size: 24rpx;
  color: var(--text-4);
}

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
  align-items: center;
  justify-content: space-between;
  margin-bottom: 12rpx;
}
.sheet-title {
  font-size: 34rpx;
  font-weight: 600;
  color: var(--text-1);
}
.sheet-sub {
  display: block;
  font-size: 24rpx;
  color: var(--text-3);
  margin-bottom: 20rpx;
}
.sheet-close {
  display: flex;
  align-items: center;
  padding: 8rpx;
}
.remark {
  width: 100%;
  height: 180rpx;
  box-sizing: border-box;
  padding: 24rpx;
  margin-bottom: 24rpx;
  background: var(--bg-soft);
  border: 1rpx solid var(--border);
  border-radius: 8rpx;
  font-size: 28rpx;
}
.ph {
  color: var(--text-4);
}
.sheet-actions {
  display: flex;
  gap: 20rpx;
}
.sheet-actions .btn {
  flex: 1;
}
</style>
