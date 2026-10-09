<template>
  <view class="home page-pad">
    <!-- 头部 -->
    <view class="head">
      <text class="greet">{{ store.name || '老师' }}，您好</text>
      <view class="head-meta">
        <text class="head-date">{{ today }}</text>
        <StatusTag :text="roleLabel" :tone="store.role === 'admin' ? 'info' : 'neutral'" />
      </view>
    </view>

    <!-- 订阅引导（真实模式，未订阅时显示） -->
    <SubscribePrompt />

    <!-- 数据总览：2×2 统计网格 -->
    <view class="stats">
      <view class="stat card" @tap="go('devices')">
        <text class="stat-label">设备总数</text>
        <text class="stat-num">{{ stats.total }}</text>
      </view>
      <view class="stat card" @tap="go('devices')">
        <text class="stat-label">在线设备</text>
        <text class="stat-num">{{ stats.online }}</text>
      </view>
      <view class="stat card" @tap="go('devices')">
        <text class="stat-label">离线设备</text>
        <text class="stat-num">{{ stats.offline }}</text>
      </view>
      <view class="stat card" @tap="go('alerts')">
        <text class="stat-label">待处理告警</text>
        <view class="stat-num-wrap">
          <view class="stat-dot"></view>
          <text class="stat-num">{{ stats.unhandled }}</text>
        </view>
      </view>
    </view>

    <!-- 管理入口（仅管理员） -->
    <view v-if="store.role === 'admin'" class="list-group manage">
      <view class="list-cell list-cell-center" hover-class="cell-hover" @tap="goManage('bindings')">
        <view class="m-icon"><uni-icons type="person" size="20" color="var(--text-2)" /></view>
        <text class="m-label">教师教室绑定</text>
        <uni-icons type="arrow-right" size="14" color="var(--text-4)" />
      </view>
      <view class="list-cell list-cell-center" hover-class="cell-hover" @tap="goManage('records')">
        <view class="m-icon"><uni-icons type="flag" size="20" color="var(--text-2)" /></view>
        <text class="m-label">进出记录</text>
        <uni-icons type="arrow-right" size="14" color="var(--text-4)" />
      </view>
      <view class="list-cell list-cell-center" hover-class="cell-hover" @tap="goManage('space-types')">
        <view class="m-icon"><uni-icons type="gear" size="20" color="var(--text-2)" /></view>
        <text class="m-label">空间类型</text>
        <uni-icons type="arrow-right" size="14" color="var(--text-4)" />
      </view>
    </view>

    <!-- 教室列表（有告警高亮） -->
    <view class="section-title">
      <text class="t">教室列表</text>
      <text class="more" @tap="go('devices')">查看全部</text>
    </view>
    <view class="list-group">
      <view
        v-for="c in classrooms"
        :key="c.space_id"
        class="list-cell list-cell-center space-type"
        :class="{ 'has-alert': c.unhandled > 0 }"
        hover-class="cell-hover"
        @tap="goClassroom(c.space_id)"
      >
        <text class="st-name">{{ c.name }}</text>
        <text class="st-count">在线 {{ c.online }}/{{ c.total }} · 待处理 {{ c.unhandled }}</text>
        <uni-icons type="arrow-right" size="14" color="var(--text-4)" />
      </view>
    </view>

    <!-- 最新告警 -->
    <view class="section-title">
      <text class="t">最新告警</text>
      <text class="more" @tap="go('alerts')">{{ alertMoreLabel }}</text>
    </view>
    <view v-if="recentAlerts.length" class="list-group">
      <view
        v-for="a in recentAlerts"
        :key="a.alert_id"
        class="list-cell"
        hover-class="cell-hover"
        @tap="goAlert(a)"
      >
        <view class="a-icon"><uni-icons :type="eventTypeIcon(a.event_type)" size="20" color="var(--text-2)" /></view>
        <view class="alert-main">
          <text class="alert-label">{{ eventLabel(a.event_type) }}</text>
          <text class="alert-sub">{{ spaceName(a.space_id) }}<text v-if="a.created_at"> · {{ fmt(a.created_at) }}</text></text>
        </view>
        <StatusTag :text="alertStatus(a.status)" :tone="statusTone(a.status)" />
      </view>
    </view>
    <EmptyState v-else title="当前没有待处理告警" desc="设备运行正常" icon="checkbox-filled" />

    <AppTabBar current="home" />
  </view>
</template>

<script>
import { api } from '../../api/index'
import { store, isLoggedIn } from '../../store/index'
import { eventTypeLabel, alertStatusLabel, alertStatusTone, formatTime } from '../../utils/format'
import { eventTypeIcon } from '../../utils/icon'
import StatusTag from '../../components/StatusTag.vue'
import EmptyState from '../../components/EmptyState.vue'
import AppTabBar from '../../components/AppTabBar.vue'
import SubscribePrompt from '../../components/SubscribePrompt.vue'

// 首页空间概览最多展示的类型数；超出部分靠「查看全部」进设备页查看。
const HOME_SPACE_TYPE_LIMIT = 3
// 首页「最新告警」最多展示的条数；超出部分点「查看全部」进告警页。
const HOME_ALERT_LIMIT = 3

export default {
  components: { StatusTag, EmptyState, AppTabBar, SubscribePrompt },
  data() {
    return {
      store,
      spaces: [],
      classrooms: [],
      stats: { total: 0, online: 0, offline: 0, unhandled: 0 },
      recentAlerts: [],
      redirecting: false,
    }
  },
  computed: {
    roleLabel() {
      return store.role === 'admin' ? '管理员' : '教师'
    },
    today() {
      const d = new Date()
      const w = ['日', '一', '二', '三', '四', '五', '六'][d.getDay()]
      return `${d.getMonth() + 1}月${d.getDate()}日 周${w}`
    },
    alertMoreCount() {
      return Math.max(0, this.stats.unhandled - HOME_ALERT_LIMIT)
    },
    alertMoreLabel() {
      return this.alertMoreCount > 0 ? `查看全部（+${this.alertMoreCount}）` : '查看全部'
    },
  },
  onShow() {
    if (!isLoggedIn()) {
      uni.reLaunch({ url: '/pages/login/login' })
      return
    }
    // 教师首页即「我的教室」；误入总览时回跳，标志位避免 onShow 重复触发。
    if (store.role === 'teacher' && !this.redirecting) {
      this.redirecting = true
      uni.reLaunch({ url: '/pages/classroom/classroom' })
      return
    }
    this.load()
  },
  onPullDownRefresh() {
    this.load()
  },
  methods: {
    eventLabel: eventTypeLabel,
    eventTypeIcon,
    alertStatus: alertStatusLabel,
    statusTone: alertStatusTone,
    fmt: formatTime,
    spaceName(id) {
      const s = this.spaces.find((x) => x.space_id === id)
      return s ? s.name : id
    },
    async load() {
      try {
        const spaces = await api.spaces()
        this.spaces = spaces
        // N+1 技术债：首页逐教室拉 devices 聚合全局统计。详见 docs/n+1-设备聚合-技术债.md（倾向新增 /devices/summary）。
        const [deviceLists, statsRes] = await Promise.all([
          Promise.all(spaces.map((s) => api.devices(s.space_id))),
          api.alertStats(),
        ])
        // by_space_unhandled 是数组 [{space_id, count}]，转 map 便于查。
        const unhandledMap = {}
        const unhandledArr = (statsRes && statsRes.by_space_unhandled) || []
        unhandledArr.forEach((x) => { unhandledMap[x.space_id] = x.count })

        let total = 0
        let online = 0
        const classrooms = spaces.map((s, i) => {
          const ds = (deviceLists[i] && deviceLists[i].devices) || []
          const on = ds.filter((d) => d.status === 'online').length
          total += ds.length
          online += on
          return {
            space_id: s.space_id,
            name: s.name,
            type: s.type,
            online: on,
            total: ds.length,
            offline: ds.length - on,
            unhandled: unhandledMap[s.space_id] || 0,
          }
        })
        // 排序：有告警教室排前，同组按未处理数降序（空教室也显示 0/0/0）。
        classrooms.sort((a, b) => {
          const aAlert = a.unhandled > 0 ? 1 : 0
          const bAlert = b.unhandled > 0 ? 1 : 0
          if (aAlert !== bAlert) return bAlert - aAlert
          return b.unhandled - a.unhandled
        })
        this.classrooms = classrooms
        this.stats.total = total
        this.stats.online = online
        this.stats.offline = total - online

        const alerts = await api.alerts({ status: 'unhandled', page: 1, page_size: HOME_ALERT_LIMIT })
        this.stats.unhandled = (alerts && alerts.total) || 0
        store.unhandledAlerts = (alerts && alerts.total) || 0
        this.recentAlerts = (alerts && alerts.alerts) || []
      } catch (e) {
        uni.showToast({ title: e.message || '加载失败', icon: 'none' })
      } finally {
        uni.stopPullDownRefresh()
      }
    },
    go(page) {
      uni.reLaunch({ url: '/pages/' + page + '/' + page })
    },
    goManage(type) {
      if (type === 'bindings') {
        uni.navigateTo({ url: '/pages/admin/space-bindings' })
      } else if (type === 'records') {
        uni.navigateTo({ url: '/pages/records/records' })
      } else if (type === 'space-types') {
        uni.navigateTo({ url: '/pages/admin/space-types' })
      }
    },
    goClassroom(spaceId) {
      uni.navigateTo({ url: '/pages/devices/devices?space_id=' + encodeURIComponent(spaceId) })
    },
    goAlert(a) {
      // 单条告警直达详情：先入告警列表再透传 goto，返回时依次回到告警列表 → 首页。
      uni.navigateTo({ url: '/pages/alerts/alerts?goto=' + encodeURIComponent(a.alert_id) })
    },
  },
}
</script>

<style scoped>
.home {
  /* 注意：bottom 交给 .page-pad 让位给 tabbar，这里不要用 padding shorthand */
  padding-top: 20rpx;
  padding-left: 20rpx;
  padding-right: 20rpx;
}

/* 头部 */
.head {
  display: flex;
  flex-direction: column;
  padding: 4rpx 4rpx 0;
}
.greet {
  font-size: 32rpx;
  font-weight: 600;
  color: var(--text-1);
}
.head-meta {
  display: flex;
  align-items: center;
  margin-top: 8rpx;
}
.head-date {
  font-size: 24rpx;
  color: var(--text-3);
  margin-right: 16rpx;
}

/* 数据总览：2×2 统计网格 */
.stats {
  display: flex;
  flex-wrap: wrap;
  margin-top: 20rpx;
}
.stat {
  width: calc(50% - 8rpx);
  box-sizing: border-box;
  padding: 20rpx 22rpx;
  margin-bottom: 16rpx;
}
.stat:nth-child(odd) {
  margin-right: 16rpx;
}
.stat-label {
  display: block;
  font-size: 22rpx;
  color: var(--text-3);
}
.stat-num {
  display: block;
  margin-top: 8rpx;
  font-size: 40rpx;
  font-weight: 600;
  color: var(--text-1);
  line-height: 1;
}
.stat-num-wrap {
  display: flex;
  align-items: center;
  margin-top: 8rpx;
}
.stat-num-wrap .stat-num {
  margin-top: 0;
}
.stat-dot {
  width: 12rpx;
  height: 12rpx;
  border-radius: 50%;
  margin-right: 10rpx;
  background: var(--warning);
}

/* 管理入口（仅管理员） */
.manage {
  margin-top: 20rpx;
}
.m-icon {
  width: 64rpx;
  height: 64rpx;
  border-radius: 8rpx;
  margin-right: 20rpx;
  flex-shrink: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  color: var(--text-2);
  background: var(--info-bg);
}
.m-label {
  flex: 1;
  font-size: 28rpx;
  color: var(--text-1);
}

/* 空间概览（按类型）：一行一类 */
.space-type.has-alert {
  background: var(--warning-bg);
}
.st-name {
  font-size: 28rpx;
  color: var(--text-1);
}
.st-count {
  flex: 1;
  margin-left: 20rpx;
  font-size: 22rpx;
  color: var(--text-3);
}

/* 告警列表 */
.a-icon {
  width: 72rpx;
  height: 72rpx;
  border-radius: 8rpx;
  margin-right: 22rpx;
  flex-shrink: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  color: var(--text-2);
  background: var(--info-bg);
}
.alert-main {
  flex: 1;
  display: flex;
  flex-direction: column;
}
.alert-label {
  font-size: 28rpx;
  color: var(--text-1);
}
.alert-sub {
  margin-top: 4rpx;
  font-size: 22rpx;
  color: var(--text-3);
}
</style>
