<template>
  <view class="detail">
    <!-- 告警摘要 -->
    <view v-if="alert" class="card summary">
      <view class="s-head">
        <text class="s-label">{{ eventLabel(alert.event_type) }}</text>
        <StatusTag :text="statusLabel(alert.status)" :tone="statusTone(alert.status)" />
      </view>
      <text class="s-sub">
        {{ spaceName(alert.space_id) }}<text v-if="alert.created_at"> · 产生于 {{ fmt(alert.created_at) }}</text>
      </text>
      <text v-if="alert.operator_name" class="s-op">当前处理人：{{ alert.operator_name }}</text>
      <view v-if="hasDevice" class="s-actions">
        <button class="btn btn-primary" @tap="goControl">远程控制设备</button>
      </view>
    </view>

    <!-- 处理过程时间线（仅 admin；教师深链只展示告警摘要） -->
    <view v-if="isAdmin" class="section-title"><text class="t">处理过程</text></view>
    <view v-if="isAdmin && alert" class="card tl">
      <!-- 告警产生锚点：假设告警产生时必为 unhandled（producer 恒以 unhandled 落库）；
           若将来状态机改动，此文案需同步。 -->
      <view class="tl-row">
        <view class="tl-badge"><StatusTag text="待处理" tone="warning" /></view>
        <view class="tl-main">
          <text class="tl-title">告警产生</text>
          <text class="tl-sub">{{ fmt(alert.created_at) }}</text>
        </view>
      </view>
      <view v-for="e in timeline" :key="e.id" class="tl-row">
        <view class="tl-badge"><StatusTag :text="statusLabel(e.status)" :tone="statusTone(e.status)" /></view>
        <view class="tl-main">
          <text class="tl-title">{{ e.operator_name ? e.operator_name + ' · ' + statusLabel(e.status) : statusLabel(e.status) }}</text>
          <text v-if="e.remark" class="tl-remark">备注：{{ e.remark }}</text>
          <text class="tl-sub">{{ fmt(e.created_at) }}</text>
        </view>
      </view>
      <view v-if="!timeline.length" class="tl-empty">暂无处理记录</view>
    </view>

    <EmptyState v-if="!alert && !loading" title="告警不存在或已删除" desc="请返回告警列表重试" icon="checkbox-filled" />
    <view v-if="!alert && !loading" class="empty-actions">
      <button class="btn btn-primary" @tap="backToList">返回告警列表</button>
    </view>
  </view>
</template>

<script>
import { api } from '../../api/index'
import { store, isLoggedIn } from '../../store/index'
import { eventTypeLabel, alertStatusLabel, alertStatusTone, formatTime } from '../../utils/format'
import { eventTypeIcon } from '../../utils/icon'
import { alertErrorMessage } from '../../utils/alertError'
import StatusTag from '../../components/StatusTag.vue'
import EmptyState from '../../components/EmptyState.vue'

export default {
  components: { StatusTag, EmptyState },
  data() {
    return {
      id: '',
      spaces: [],
      alert: null,
      timeline: [],
      loading: false,
    }
  },
  onLoad(option) {
    if (!isLoggedIn()) {
      uni.reLaunch({ url: '/pages/login/login' })
      return
    }
    this.id = option.id || ''
    if (!this.id) {
      // 空 id（异常深链/手动进入）：回退列表，勿用空 id 调接口。
      uni.redirectTo({ url: '/pages/alerts/alerts' })
      return
    }
    this.load()
  },
  computed: {
    hasDevice() { return !!(this.alert && this.alert.device_id) },
    isAdmin() { return store.role === 'admin' },
  },
  methods: {
    eventLabel: eventTypeLabel,
    eventTypeIcon,
    statusLabel: alertStatusLabel,
    statusTone: alertStatusTone,
    fmt: formatTime,
    spaceName(id) {
      const s = this.spaces.find((x) => x.space_id === id)
      return s ? s.name : id
    },
    backToList() {
      uni.redirectTo({ url: '/pages/alerts/alerts' })
    },
    goControl() {
      const a = this.alert
      uni.navigateTo({
        url: '/pages/devices/devices?space_id=' + encodeURIComponent(a.space_id)
             + '&device_id=' + encodeURIComponent(a.device_id || '')
             + '&from_alert=' + encodeURIComponent(a.alert_id || '')
      })
    },
    async load() {
      if (!this.id) return
      this.loading = true
      try {
        const [spaces, res] = await Promise.all([
          api.spaces(),
          this.isAdmin ? api.alertTimeline(this.id) : api.alertDetail(this.id),
        ])
        this.spaces = spaces || []
        this.alert = (res && res.alert) || null
        this.timeline = this.isAdmin ? ((res && res.timeline) || []) : []
      } catch (e) {
        // 404（教师越权 / 告警不存在）走空态，不弹错误提示；其它错误才提示。
        if (!(e && e.status === 404)) {
          uni.showToast({ title: alertErrorMessage(e) || '加载失败', icon: 'none' })
        }
      } finally {
        this.loading = false
      }
    },
  },
}
</script>

<style scoped>
.detail {
  padding: 24rpx;
}
.summary {
  padding: 28rpx 28rpx;
}
.s-head {
  display: flex;
  align-items: center;
  justify-content: space-between;
}
.s-label {
  font-size: 32rpx;
  font-weight: 600;
  color: var(--text-1);
}
.s-sub {
  display: block;
  margin-top: 14rpx;
  font-size: 24rpx;
  color: var(--text-3);
}
.s-op {
  display: block;
  margin-top: 10rpx;
  font-size: 24rpx;
  color: var(--text-2);
}
.s-actions {
  margin-top: 24rpx;
}
.tl {
  padding: 12rpx 28rpx;
}
.tl-row {
  display: flex;
  padding: 24rpx 0;
  border-bottom: 1rpx solid var(--border-lighter);
}
.tl-row:last-child {
  border-bottom: none;
}
.tl-badge {
  width: 120rpx;
  flex-shrink: 0;
}
.tl-main {
  flex: 1;
  display: flex;
  flex-direction: column;
}
.tl-title {
  font-size: 28rpx;
  font-weight: 500;
  color: var(--text-1);
}
.tl-remark {
  margin-top: 8rpx;
  font-size: 24rpx;
  color: var(--text-3);
}
.tl-sub {
  margin-top: 6rpx;
  font-size: 22rpx;
  color: var(--text-4);
}
.tl-empty {
  padding: 32rpx 0;
  text-align: center;
  font-size: 24rpx;
  color: var(--text-4);
}
.empty-actions {
  display: flex;
  justify-content: center;
  padding: 0 48rpx 40rpx;
}
.empty-actions .btn {
  width: 320rpx;
}
</style>
