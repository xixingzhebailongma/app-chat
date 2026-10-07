<template>
  <view class="records page-pad">
    <!-- 日期 + 教室筛选 -->
    <view class="filters">
      <picker mode="date" :value="date" @change="onDateChange">
        <view class="filter card">
          <text class="filter-label">{{ date }}</text>
          <uni-icons type="calendar" size="16" color="var(--text-3)" />
        </view>
      </picker>
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
        :range="authTabs"
        range-key="label"
        @change="onAuthTypeChange"
      >
        <view class="filter card">
          <text class="filter-label">{{ currentAuthTypeLabel }}</text>
          <uni-icons type="arrow-down" size="16" color="var(--text-3)" />
        </view>
      </picker>
    </view>

    <!-- 汇总 -->
    <view class="summary">共 <text class="n">{{ total }}</text> 条记录</view>

    <!-- 记录列表 -->
    <view v-if="records.length" class="list-group">
      <view
        v-for="(r, i) in records"
        :key="r.time + r.space_id + r.name + i"
        class="list-cell list-cell-center"
      >
        <view class="r-icon"><uni-icons type="person" size="22" color="var(--text-2)" /></view>
        <view class="r-main">
          <view class="r-title">
            <text class="r-label">{{ r.name || '未识别' }}</text>
            <StatusTag :text="resultLabel(r.result)" :tone="resultTone(r.result)" />
          </view>
          <text class="r-sub">{{ spaceName(r.space_id) }} · {{ authLabel(r.auth_type) }} · {{ fmt(r.time) }}</text>
        </view>
      </view>
      <view v-if="hasMore" class="load-more" @tap="loadMore">加载更多</view>
      <view v-else class="load-end">没有更多了</view>
    </view>
    <EmptyState v-else title="该日暂无进出记录" desc="可切换日期或教室查看" icon="flag" />
  </view>
</template>

<script>
import { api } from '../../api/index'
import { store, isLoggedIn } from '../../store/index'
import { formatTime } from '../../utils/format'
import StatusTag from '../../components/StatusTag.vue'
import EmptyState from '../../components/EmptyState.vue'

function todayStr() {
  const d = new Date()
  const p = (n) => (n < 10 ? '0' + n : '' + n)
  return `${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())}`
}

export default {
  components: { StatusTag, EmptyState },
  data() {
    return {
      store,
      spaces: [],
      date: todayStr(),
      current: '', // '' = 全部（teacher 由服务端收窄到绑定教室）
      authType: '', // '' = 全部方式 / face / card
      records: [],
      total: 0,
      page: 1,
      pageSize: 20,
      hasMore: false,
      loading: false,
      inited: false,
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
      const s = this.spaceTabs.find((x) => x.key === this.current)
      return s ? s.label : '全部'
    },
    authTabs() {
      return [
        { key: '', label: '全部方式' },
        { key: 'face', label: '人脸' },
        { key: 'card', label: '刷卡' },
      ]
    },
    currentAuthTypeLabel() {
      const a = this.authTabs.find((x) => x.key === this.authType)
      return a ? a.label : '全部方式'
    },
  },
  onLoad(options) {
    // 入口（如「今日进出」）可带入教室与方式，落到对应空间/过滤。
    this.initialSpace = (options && options.space_id) || ''
    this.authType = (options && options.auth_type) || ''
  },
  onShow() {
    if (!isLoggedIn()) {
      uni.reLaunch({ url: '/pages/login/login' })
      return
    }
    this.load()
  },
  onPullDownRefresh() {
    this.fetchRecords(true)
  },
  onReachBottom() {
    this.loadMore()
  },
  methods: {
    fmt: formatTime,
    resultLabel(result) {
      return result === 'login' ? '进入' : '拒绝'
    },
    resultTone(result) {
      return result === 'login' ? 'success' : 'danger'
    },
    authLabel(auth) {
      return auth === 'card' ? '刷卡' : '人脸'
    },
    spaceName(id) {
      const s = this.spaces.find((x) => x.space_id === id)
      return s ? s.name : id
    },
    async load() {
      try {
        const spaces = await api.spaces()
        this.spaces = spaces
        if (!this.inited) {
          this.current = this.initialSpace || ''
          this.inited = true
        }
        await this.fetchRecords(true)
      } catch (e) {
        uni.showToast({ title: e.message || '加载失败', icon: 'none' })
        uni.stopPullDownRefresh()
      }
    },
    onDateChange(e) {
      this.date = e.detail.value
      this.fetchRecords(true)
    },
    onSpaceChange(e) {
      const idx = Number(e.detail.value)
      const s = this.spaceTabs[idx]
      if (s && s.key !== this.current) {
        this.current = s.key
        this.fetchRecords(true)
      }
    },
    onAuthTypeChange(e) {
      const idx = Number(e.detail.value)
      const a = this.authTabs[idx]
      if (a && a.key !== this.authType) {
        this.authType = a.key
        this.fetchRecords(true)
      }
    },
    async fetchRecords(reset) {
      if (this.loading) return
      if (reset) {
        this.page = 1
        this.records = []
      }
      this.loading = true
      try {
        const r = await api.accessRecords(
          this.current || '',
          this.date,
          this.page,
          this.pageSize,
          this.authType
        )
        const rows = r.records || []
        this.records = reset ? rows : this.records.concat(rows)
        this.total = r.total || 0
        this.hasMore = !!r.has_more
      } catch (e) {
        uni.showToast({ title: e.message || '加载失败', icon: 'none' })
      } finally {
        this.loading = false
        uni.stopPullDownRefresh()
      }
    },
    loadMore() {
      if (!this.hasMore || this.loading) return
      this.page += 1
      this.fetchRecords(false)
    },
  },
}
</script>

<style scoped>
.records {
  padding-top: 24rpx;
  padding-left: 24rpx;
  padding-right: 24rpx;
}

.filters {
  display: flex;
  gap: 20rpx;
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
  margin: 24rpx 4rpx 4rpx;
  font-size: 24rpx;
  color: var(--text-3);
}
.summary .n {
  font-weight: 600;
}

.list-group {
  margin-top: 12rpx;
}
.r-icon {
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
.r-main {
  flex: 1;
  display: flex;
  flex-direction: column;
}
.r-title {
  display: flex;
  align-items: center;
}
.r-label {
  font-size: 30rpx;
  font-weight: 600;
  color: var(--text-1);
  margin-right: 14rpx;
}
.r-sub {
  margin-top: 6rpx;
  font-size: 22rpx;
  color: var(--text-3);
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
</style>
