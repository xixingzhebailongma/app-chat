<template>
  <view class="home page-pad">
    <!-- 头部 Hero -->
    <view class="head card">
      <view class="head-row">
        <text class="greet">{{ store.name || '老师' }}，您好</text>
        <StatusTag :text="roleLabel" :tone="store.role === 'admin' ? 'info' : 'neutral'" />
      </view>
      <text class="head-sub">{{ summary }}</text>
    </view>

    <!-- 订阅引导（真实模式，未订阅时显示） -->
    <SubscribePrompt />

    <!-- 数据总览：告警优先（大卡），其余在线指标收成一横排 -->
    <view class="stats">
      <view
        class="stat-alert card"
        :class="{ 'stat-alert--active': stats.unhandled > 0 }"
        @tap="go('alerts')"
      >
        <view class="stat-alert-main">
          <text class="stat-alert-label">待处理告警</text>
          <text class="stat-alert-num num">{{ stats.unhandled }}</text>
        </view>
        <view class="stat-alert-side">
          <text class="stat-alert-hint">{{ stats.unhandled > 0 ? '点此进入处置' : '当前无待处理' }}</text>
          <view class="stat-alert-btn">
            <uni-icons type="right" size="14" color="#ffffff" />
          </view>
        </view>
      </view>
      <view class="stat-row">
        <view class="stat-mini" @tap="go('devices')">
          <text class="stat-mini-label">设备总数</text>
          <text class="stat-mini-num num">{{ stats.loaded ? stats.total : '—' }}</text>
        </view>
        <view class="stat-mini" @tap="go('devices')">
          <text class="stat-mini-label">在线设备</text>
          <text class="stat-mini-num num stat-mini-num--on">{{ stats.loaded ? stats.online : '—' }}</text>
        </view>
        <view class="stat-mini" @tap="go('devices')">
          <text class="stat-mini-label">离线设备</text>
          <text class="stat-mini-num num">{{ stats.loaded ? stats.offline : '—' }}</text>
        </view>
      </view>
    </view>

    <!-- 管理入口（仅管理员） -->
    <view v-if="store.role === 'admin'" class="list-group manage">
      <view class="list-cell list-cell-center" hover-class="cell-hover" @tap="goManage('spaces')">
        <view class="m-icon"><uni-icons type="home" size="20" color="var(--brand)" /></view>
        <text class="m-label">教室管理</text>
        <uni-icons type="arrow-right" size="14" color="var(--text-4)" />
      </view>
      <view class="list-cell list-cell-center" hover-class="cell-hover" @tap="goManage('bindings')">
        <view class="m-icon"><uni-icons type="person" size="20" color="var(--brand)" /></view>
        <text class="m-label">教师教室绑定</text>
        <uni-icons type="arrow-right" size="14" color="var(--text-4)" />
      </view>
      <view class="list-cell list-cell-center" hover-class="cell-hover" @tap="goManage('records')">
        <view class="m-icon"><uni-icons type="flag" size="20" color="var(--brand)" /></view>
        <text class="m-label">进出记录</text>
        <uni-icons type="arrow-right" size="14" color="var(--text-4)" />
      </view>
      <view class="list-cell list-cell-center" hover-class="cell-hover" @tap="goManage('space-types')">
        <view class="m-icon"><uni-icons type="gear" size="20" color="var(--brand)" /></view>
        <text class="m-label">空间类型</text>
        <uni-icons type="arrow-right" size="14" color="var(--text-4)" />
      </view>
    </view>

    <!-- 教室入口：打开聚合面板（按类型分组 + 搜索 + 在线/告警状态），点教室直达设备页 -->
    <view class="section-title">
      <text class="t">进入教室</text>
      <text class="more">{{ entrySummary }}</text>
    </view>
    <view v-if="classrooms.length" class="home__pick" hover-class="cell-hover" @tap="openSheet">
      <view class="home__pick-ic"><uni-icons type="home" size="18" color="var(--brand)" /></view>
      <view class="home__pick-main">
        <text class="home__pick-title">选择教室，进入设备控制</text>
        <text class="home__pick-sub">按类型分组，有告警的教室标红置顶</text>
      </view>
      <uni-icons type="arrow-down" size="16" color="var(--text-3)" />
    </view>
    <view v-else class="home__empty">暂无教室</view>

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
        <view class="a-icon"><uni-icons :type="eventTypeIcon(a.event_type)" size="20" color="var(--brand)" /></view>
        <view class="alert-main">
          <text class="alert-label">{{ eventLabel(a.event_type) }}</text>
          <text class="alert-sub">{{ spaceName(a.space_id) }}<text v-if="a.created_at"> · {{ fmt(a.created_at) }}</text></text>
        </view>
        <StatusTag :text="alertStatus(a.status)" :tone="statusTone(a.status)" />
      </view>
    </view>
    <EmptyState v-else title="当前没有待处理告警" desc="设备运行正常" icon="checkbox-filled" />

    <!-- 教室聚合面板：搜索 + 按类型分组折叠，每行带在线数/待处理角标，点行直达该教室设备页 -->
    <view v-if="sheetVisible" class="rs-mask" @tap="closeSheet"></view>
    <view class="rs-sheet" :class="{ show: sheetVisible }" @tap.stop>
      <view class="rs-head">
        <view class="rs-head-main">
          <text class="rs-title">选择教室</text>
          <text class="rs-sub">{{ classrooms.length }} 间<text v-if="stats.unhandled > 0"> · {{ stats.unhandled }} 条待处理</text></text>
        </view>
        <view class="rs-close" @tap="closeSheet"><uni-icons type="close" size="18" color="var(--text-4)" /></view>
      </view>
      <view class="rs-search">
        <view class="rs-search-ico"><uni-icons type="search" size="16" color="var(--text-3)" /></view>
        <input
          v-model="keyword"
          class="rs-search-input"
          placeholder="搜索教室名称"
          placeholder-class="ph"
        />
        <view v-if="keyword" class="rs-search-clear" @tap="keyword = ''">
          <uni-icons type="clear" size="16" color="var(--text-4)" />
        </view>
      </view>
      <scroll-view scroll-y class="rs-list">
        <view v-for="g in filteredGroups" :key="g.type" class="rs-group">
          <view class="rs-group-head" @tap="toggleGroup(g.type)">
            <text class="rs-group-name">{{ g.label }}</text>
            <text class="rs-group-count">{{ g.rooms.length }} 间</text>
            <text v-if="g.alertCount" class="rs-group-alert">{{ g.alertCount }} 条告警</text>
            <uni-icons :type="groupHidden(g.type) ? 'arrow-down' : 'arrow-up'" size="14" color="var(--text-3)" />
          </view>
          <view v-show="!groupHidden(g.type)">
            <view
              v-for="c in g.rooms"
              :key="c.space_id"
              class="rs-room"
              :class="{ 'rs-room--alert': c.unhandled > 0 }"
              hover-class="cell-hover"
              @tap="onPickRoom(c)"
            >
              <view class="rs-room-main">
                <text class="rs-room-name">{{ c.name }}</text>
                <text class="rs-room-sub">{{ roomStatusText(c) }}</text>
              </view>
              <text v-if="c.unhandled" class="rs-badge">待处理 {{ c.unhandled }}</text>
              <uni-icons type="arrow-right" size="14" color="var(--text-4)" />
            </view>
          </view>
        </view>
        <view v-if="!filteredGroups.length" class="rs-empty">没有匹配「{{ keyword }}」的教室</view>
        <view class="rs-list-foot" />
      </scroll-view>
    </view>

    <AppTabBar current="home" />
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
  spaceTypeLabel,
  orderedSpaceTypeKeys,
} from '../../utils/format'
import { eventTypeIcon } from '../../utils/icon'
import StatusTag from '../../components/StatusTag.vue'
import EmptyState from '../../components/EmptyState.vue'
import AppTabBar from '../../components/AppTabBar.vue'
import SubscribePrompt from '../../components/SubscribePrompt.vue'

// 首页「最新告警」最多展示的条数；超出部分点「查看全部」进告警页。
const HOME_ALERT_LIMIT = 3

/* ----------------------------------------------------------------------------
 * 设备聚合缓存（缓解 N+1 技术债，详见 docs/n+1-设备聚合-技术债.md）
 * 首页要显示每间教室的在线/总数，只能逐教室调 devices（N 间 = N 次请求）。
 * 后端不能动，前端做两件事：
 *   1. 懒加载：首屏只拉教室列表 + 告警统计（含每教室未处理数），设备在线数
 *      不阻塞首屏，后台静默拉取；打开教室面板时数据通常已经就绪。
 *   2. 短时缓存：30s 内切 Tab / 返回首页零请求，直接用上次结果。
 * 缓存挂在模块级：页面销毁重建（切 Tab）也能命中。
 * ------------------------------------------------------------------------- */
const DEVICE_CACHE_TTL = 30 * 1000
const deviceCache = {
  ts: 0, // 上次成功聚合的时间戳
  bySpace: {}, // { space_id: { total, online } }
  inflight: null, // 进行中的聚合请求，多处等待时复用同一个 Promise
}

export default {
  components: { StatusTag, EmptyState, AppTabBar, SubscribePrompt },
  data() {
    return {
      store,
      spaces: [],
      classrooms: [],
      // loaded=false 时设备总数/在线/离线先显示「—」，聚合请求回来后再填
      stats: { loaded: false, total: 0, online: 0, offline: 0, unhandled: 0 },
      recentAlerts: [],
      redirecting: false,
      // 教室聚合面板
      sheetVisible: false,
      keyword: '',
      collapsed: {}, // { [空间类型]: 是否折叠 }，默认全部展开
      devicesLoading: false,
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
    summary() {
      // 设备聚合是懒加载的，回来前头部只报日期与待处理数，避免显示 0 造成误读
      const devicePart = this.stats.loaded
        ? `共 ${this.stats.total} 台设备，${this.stats.online} 台在线，`
        : ''
      return `${this.today} · ${devicePart}${this.stats.unhandled} 条待处理`
    },
    // 入口标题右侧：设备数没回来前用「间数 + 待处理」兜底
    entrySummary() {
      const base = this.classrooms.length + ' 间'
      if (this.stats.loaded) return base + ' · ' + this.stats.total + ' 台设备'
      if (this.stats.unhandled > 0) return base + ' · ' + this.stats.unhandled + ' 条待处理'
      return base
    },
    // 教室聚合面板：按空间类型分组（顺序沿用 format.orderedSpaceTypeKeys），
    // 关键字过滤组内教室，空组整组隐藏；classrooms 已按「有告警在前」排过序，组内沿用
    filteredGroups() {
      const k = this.keyword.trim().toLowerCase()
      const rooms = k
        ? this.classrooms.filter((c) => c.name.toLowerCase().indexOf(k) >= 0)
        : this.classrooms
      return orderedSpaceTypeKeys(rooms.map((c) => c.type)).map((type) => {
        const list = rooms.filter((c) => c.type === type)
        return {
          type,
          label: spaceTypeLabel(type),
          rooms: list,
          alertCount: list.reduce((n, c) => n + c.unhandled, 0),
        }
      })
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
    // 下拉是用户主动刷新：忽略缓存，强制重新聚合
    this.load(true)
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
    async load(force) {
      try {
        // 首屏只等三个轻量请求：教室列表、告警统计（含每教室未处理数）、最新 3 条告警。
        // 设备在线聚合（N 次 devices 请求）放到后台，不阻塞首屏。
        const [spaces, statsRes, alerts] = await Promise.all([
          api.spaces(),
          api.alertStats(),
          api.alerts({ status: 'unhandled', page: 1, page_size: HOME_ALERT_LIMIT }),
        ])
        this.spaces = spaces
        // by_space_unhandled 是数组 [{space_id, count}]，转 map 便于查。
        const unhandledMap = {}
        const unhandledArr = (statsRes && statsRes.by_space_unhandled) || []
        unhandledArr.forEach((x) => { unhandledMap[x.space_id] = x.count })

        // 先出教室骨架：在线/总数留空（null），告警数立即可用；
        // 若模块缓存里有历史聚合结果，先把旧数字画上，避免切回来时闪「—」。
        const hasCache = Object.keys(deviceCache.bySpace).length > 0
        const classrooms = spaces.map((s) => {
          const agg = deviceCache.bySpace[s.space_id]
          return {
            space_id: s.space_id,
            name: s.name,
            type: s.type,
            online: agg ? agg.online : null,
            total: agg ? agg.total : null,
            offline: agg ? agg.total - agg.online : null,
            unhandled: unhandledMap[s.space_id] || 0,
          }
        })
        // 排序：有告警教室排前，同组按未处理数降序。
        classrooms.sort((a, b) => {
          const aAlert = a.unhandled > 0 ? 1 : 0
          const bAlert = b.unhandled > 0 ? 1 : 0
          if (aAlert !== bAlert) return bAlert - aAlert
          return b.unhandled - a.unhandled
        })
        this.classrooms = classrooms
        if (hasCache) this.applyDeviceAgg(deviceCache.bySpace)

        this.stats.unhandled = (alerts && alerts.total) || 0
        store.unhandledAlerts = (alerts && alerts.total) || 0
        this.recentAlerts = (alerts && alerts.alerts) || []

        // 设备聚合：后台静默跑（缓存新鲜时零请求），不 await。
        this.refreshDevices(spaces, force)
      } catch (e) {
        uni.showToast({ title: e.message || '加载失败', icon: 'none' })
      } finally {
        uni.stopPullDownRefresh()
      }
    },
    /* 把聚合结果 {space_id: {total, online}} 刷到教室行与头部统计 */
    applyDeviceAgg(map) {
      let total = 0
      let online = 0
      this.classrooms = this.classrooms.map((c) => {
        const agg = map[c.space_id]
        if (!agg) return c
        total += agg.total
        online += agg.online
        return { ...c, online: agg.online, total: agg.total, offline: agg.total - agg.online }
      })
      this.stats.total = total
      this.stats.online = online
      this.stats.offline = total - online
      this.stats.loaded = true
    },
    /* 逐教室聚合设备在线数：30s 缓存 + 进行中请求复用，避免并发重复 fan-out */
    async refreshDevices(spaces, force) {
      const fresh =
        !force &&
        Date.now() - deviceCache.ts < DEVICE_CACHE_TTL &&
        Object.keys(deviceCache.bySpace).length > 0
      if (fresh) {
        this.applyDeviceAgg(deviceCache.bySpace)
        return
      }
      if (deviceCache.inflight) {
        try {
          await deviceCache.inflight
          // 聚合由发起方写入缓存；这里只在成功（ts 已更新）后补画一次
          if (deviceCache.ts) this.applyDeviceAgg(deviceCache.bySpace)
        } catch (e) { /* 错误提示已由发起方弹过 */ }
        return
      }
      this.devicesLoading = true
      const task = Promise.all(spaces.map((s) => api.devices(s.space_id)))
        .then((lists) => {
          const map = {}
          spaces.forEach((s, i) => {
            const ds = (lists[i] && lists[i].devices) || []
            map[s.space_id] = {
              total: ds.length,
              online: ds.filter((d) => d.status === 'online').length,
            }
          })
          deviceCache.bySpace = map
          deviceCache.ts = Date.now()
          this.applyDeviceAgg(map)
        })
        .catch((e) => {
          uni.showToast({ title: e.message || '设备状态加载失败', icon: 'none' })
        })
        .finally(() => {
          deviceCache.inflight = null
          this.devicesLoading = false
        })
      deviceCache.inflight = task
      await task
    },
    go(page) {
      uni.reLaunch({ url: '/pages/' + page + '/' + page })
    },
    goManage(type) {
      if (type === 'spaces') {
        uni.navigateTo({ url: '/pages/admin/spaces' })
      } else if (type === 'bindings') {
        uni.navigateTo({ url: '/pages/admin/space-bindings' })
      } else if (type === 'records') {
        uni.navigateTo({ url: '/pages/records/records' })
      } else if (type === 'space-types') {
        uni.navigateTo({ url: '/pages/admin/space-types' })
      }
    },
    /* ---------------- 教室聚合面板 ---------------- */
    openSheet() {
      this.sheetVisible = true
      // 打开面板时若数据陈旧/未加载，补一次聚合（进行中会自动复用 inflight）
      const stale =
        !deviceCache.ts ||
        (!this.devicesLoading && Date.now() - deviceCache.ts >= DEVICE_CACHE_TTL)
      if (stale) this.refreshDevices(this.spaces, false)
    },
    closeSheet() {
      this.sheetVisible = false
    },
    toggleGroup(type) {
      this.collapsed = { ...this.collapsed, [type]: !this.collapsed[type] }
    },
    // 搜索中强制全部展开，否则折叠着看不到匹配结果
    groupHidden(type) {
      return !this.keyword.trim() && !!this.collapsed[type]
    },
    onPickRoom(c) {
      this.sheetVisible = false
      this.goClassroom(c.space_id)
    },
    roomStatusText(c) {
      if (c.total == null) return this.devicesLoading ? '设备状态加载中…' : '点击进入查看设备'
      const parts = ['在线 ' + c.online + '/' + c.total]
      if (c.offline > 0) parts.push('离线 ' + c.offline)
      return parts.join(' · ')
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
  padding-left: 36rpx;
  padding-right: 36rpx;
  /* 顶部一段淡蓝渐变，全站统一 */
  /* 晴昼：环境光双径向叠层（对齐 qingzhou body） */
  background:
    radial-gradient(ellipse at 30% -10%, var(--ambient-a), transparent 55%),
    radial-gradient(ellipse at 95% 70%, var(--ambient-b), transparent 52%),
    var(--bg-page);
}

/* 头部 Hero：品牌蓝渐变卡 + 白字，作为管理员首屏的主视觉 */
.head {
  display: flex;
  flex-direction: column;
  padding: 28rpx 28rpx 24rpx;
  margin-bottom: 20rpx;
  background: var(--brand-grad);
  border: none;
  border-radius: var(--radius-lg);
  box-shadow: 0 18rpx 34rpx rgba(36, 94, 112, 0.28);
}
.head-row {
  display: flex;
  align-items: center;
  justify-content: space-between;
}
.greet {
  font-size: 38rpx;
  font-weight: 700;
  color: #ffffff;
}
.head-sub {
  margin-top: 12rpx;
  font-size: 24rpx;
  color: rgba(255, 255, 255, 0.80);
}

/* 数据总览：告警大卡 + 三枚小指标（读数分主次） */
.stats {
  margin-top: 20rpx;
}
.stat-alert {
  display: flex;
  align-items: center;
  gap: 20rpx;
  padding: 26rpx 28rpx;
  background: linear-gradient(140deg, var(--bg-card), var(--bg-card-end));
  border: 1rpx solid var(--border-light);
}
.stat-alert--active {
  border-color: var(--danger-border);
  background: linear-gradient(140deg, var(--danger-bg), var(--bg-card));
}
.stat-alert-main {
  flex: 1;
  min-width: 0;
  display: flex;
  flex-direction: column;
}
.stat-alert-label {
  font-size: 24rpx;
  color: var(--text-2);
}
.stat-alert-num {
  margin-top: 6rpx;
  font-size: 56rpx;
  font-weight: 700;
  line-height: 1.05;
  color: var(--text-1);
}
.stat-alert--active .stat-alert-num {
  color: var(--danger);
}
.stat-alert-side {
  display: flex;
  flex-direction: column;
  align-items: flex-end;
  gap: 12rpx;
}
.stat-alert-hint {
  font-size: 22rpx;
  color: var(--text-3);
}
.stat-alert--active .stat-alert-hint {
  color: var(--danger);
}
.stat-alert-btn {
  width: 52rpx;
  height: 52rpx;
  border-radius: 50%;
  background: var(--brand);
  display: flex;
  align-items: center;
  justify-content: center;
}
.stat-alert--active .stat-alert-btn {
  background: var(--danger);
}
.stat-row {
  display: flex;
  gap: 16rpx;
  margin-top: 16rpx;
}
.stat-mini {
  flex: 1;
  min-width: 0;
  padding: 20rpx 22rpx;
  border-radius: var(--radius-item);
  border: 1rpx solid var(--border-light);
  background: linear-gradient(140deg, var(--bg-card), var(--bg-card-end));
}
.stat-mini-label {
  display: block;
  font-size: 22rpx;
  color: var(--text-3);
}
.stat-mini-num {
  display: block;
  margin-top: 8rpx;
  font-size: 36rpx;
  font-weight: 700;
  line-height: 1.1;
  color: var(--text-1);
}
.stat-mini-num--on {
  color: var(--success);
}

/* 管理入口（仅管理员） */
.manage {
  margin-top: 20rpx;
}
.m-icon {
  width: 64rpx;
  height: 64rpx;
  border-radius: 12rpx;
  margin-right: 20rpx;
  flex-shrink: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  color: var(--brand);
  background: var(--brand-soft);
}
.m-label {
  flex: 1;
  font-size: 28rpx;
  color: var(--text-1);
}

/* 教室下拉入口 */
.home__pick {
  display: flex;
  align-items: center;
  gap: 18rpx;
  padding: 24rpx 26rpx;
  margin: 12rpx 0 8rpx;
  background: linear-gradient(140deg, var(--bg-card), var(--bg-card-end));
  border: 1rpx solid var(--border-light);
  border-radius: var(--radius-card);
  box-shadow: var(--shadow-soft);
}
.home__pick-ic {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 64rpx;
  height: 64rpx;
  flex-shrink: 0;
  border-radius: 20rpx;
  background: var(--brand-soft);
}
.home__pick-main {
  flex: 1;
  min-width: 0;
  display: flex;
  flex-direction: column;
  gap: 6rpx;
}
.home__pick-title {
  font-size: 30rpx;
  font-weight: 600;
  color: var(--text-1);
}
.home__pick-sub {
  font-size: 22rpx;
  color: var(--text-3);
}
.home__empty {
  margin: 24rpx 0;
  text-align: center;
  font-size: 26rpx;
  color: var(--text-3);
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
  border-radius: 12rpx;
  margin-right: 22rpx;
  flex-shrink: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  color: var(--brand);
  background: var(--brand-soft);
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

/* ---------------- 教室聚合面板 ---------------- */
.rs-mask {
  position: fixed;
  left: 0;
  right: 0;
  top: 0;
  bottom: 0;
  z-index: 1000;
  background: rgba(0, 0, 0, 0.5);
}
.rs-sheet {
  position: fixed;
  left: 0;
  right: 0;
  bottom: 0;
  z-index: 1001;
  display: flex;
  flex-direction: column;
  max-height: 78vh;
  padding: 28rpx 28rpx 0;
  background: #fff;
  border-radius: 28rpx 28rpx 0 0;
  transform: translateY(100%);
  transition: transform 0.25s ease;
}
.rs-sheet.show {
  transform: translateY(0);
}
.rs-head {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  margin-bottom: 20rpx;
}
.rs-head-main {
  display: flex;
  flex-direction: column;
  gap: 6rpx;
}
.rs-title {
  font-size: 34rpx;
  font-weight: 700;
  color: var(--text-1);
}
.rs-sub {
  font-size: 24rpx;
  color: var(--text-3);
}
.rs-close {
  display: flex;
  align-items: center;
  padding: 8rpx;
}
.rs-search {
  display: flex;
  align-items: center;
  height: 72rpx;
  padding: 0 24rpx;
  margin-bottom: 16rpx;
  background: var(--bg-soft, #f5f7fa);
  border: 1rpx solid var(--border-light);
  border-radius: 999rpx;
}
.rs-search-ico {
  display: flex;
  align-items: center;
  margin-right: 12rpx;
}
.rs-search-input {
  flex: 1;
  height: 72rpx;
  font-size: 28rpx;
  color: var(--text-1);
}
.rs-search-clear {
  display: flex;
  align-items: center;
  padding: 8rpx;
}
.rs-list {
  flex: 1;
  min-height: 0;
  max-height: calc(78vh - 200rpx);
  margin: 0 -8rpx;
  padding: 0 8rpx;
}
.rs-group {
  margin-bottom: 8rpx;
}
.rs-group-head {
  display: flex;
  align-items: center;
  gap: 14rpx;
  padding: 18rpx 12rpx;
}
.rs-group-name {
  font-size: 26rpx;
  font-weight: 600;
  color: var(--text-2);
}
.rs-group-count {
  font-size: 22rpx;
  color: var(--text-3);
}
.rs-group-alert {
  font-size: 22rpx;
  color: var(--danger);
}
.rs-room {
  display: flex;
  align-items: center;
  gap: 16rpx;
  padding: 24rpx 20rpx;
  background: var(--bg-card, #fff);
  border: 1rpx solid var(--border-light);
  border-radius: 20rpx;
  margin-bottom: 12rpx;
}
/* 有待处理告警的教室：淡红底 + 红边，在列表里一眼可见 */
.rs-room--alert {
  border-color: var(--danger-border, rgba(198, 40, 40, 0.35));
  background: var(--danger-bg, #fdf2f2);
}
.rs-room-main {
  flex: 1;
  min-width: 0;
  display: flex;
  flex-direction: column;
  gap: 6rpx;
}
.rs-room-name {
  font-size: 30rpx;
  font-weight: 600;
  color: var(--text-1);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.rs-room-sub {
  font-size: 22rpx;
  color: var(--text-3);
}
.rs-badge {
  flex-shrink: 0;
  padding: 6rpx 16rpx;
  font-size: 22rpx;
  color: #fff;
  background: var(--danger);
  border-radius: 999rpx;
}
.rs-empty {
  padding: 60rpx 0;
  text-align: center;
  font-size: 26rpx;
  color: var(--text-3);
}
.rs-list-foot {
  height: calc(32rpx + env(safe-area-inset-bottom));
}
</style>
