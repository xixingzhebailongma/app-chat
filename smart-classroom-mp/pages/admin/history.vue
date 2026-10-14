<template>
  <view class="page">
    <offline-banner :ws="false" />

    <state-view
      :loading="loading"
      :error="loadError"
      :perm="permOk === false ? false : null"
      perm-text="告警历史查询为管理员功能"
      empty-text="没有符合筛选条件的告警"
      empty-desc="试试放宽时间范围或清除筛选条件"
      empty-action="清除全部筛选"
      empty-icon="search"
      @retry="load(true)"
      @perm-action="goBack"
      @empty-action="clearAll"
    >
      <!-- 筛选区 -->
      <view class="filter card">
        <view class="frow">
          <picker mode="date" :value="filters.start" @change="e => setFilter('start', e.detail.value)">
            <view class="fcell">{{ filters.start || '开始日期' }}</view>
          </picker>
          <text class="frow__dash">至</text>
          <picker mode="date" :value="filters.end" @change="e => setFilter('end', e.detail.value)">
            <view class="fcell">{{ filters.end || '结束日期' }}</view>
          </picker>
        </view>
        <view class="frow">
          <picker :range="spaceNames" @change="e => setFilter('space', e.detail.value > 0 ? store.spaces[e.detail.value - 1].space_id : '')">
            <view class="fcell fcell--full">{{ spaceLabel }}</view>
          </picker>
        </view>
        <view class="frow">
          <picker :range="typeLabels" @change="e => setFilter('type', e.detail.value > 0 ? typeKeys[e.detail.value - 1] : '')">
            <view class="fcell fcell--full">{{ typeLabel }}</view>
          </picker>
        </view>
        <button class="c-btn c-btn--primary c-btn--sm query-btn" @tap="load(true)">查 询</button>
      </view>

      <!-- 当前筛选状态：可清除的 tag -->
      <view v-if="activeTags.length" class="tags-bar">
        <view v-for="t in activeTags" :key="t.key" class="ftag" @tap="clearFilter(t.key)">
          {{ t.label }} <app-icon name="close" :size="20" :color="COLORS.primary" class="ftag__x" />
        </view>
        <text class="tags-bar__clear" @tap="clearAll">全部清除</text>
      </view>

      <!-- 结果列表 -->
      <view class="list">
        <alarm-card
          v-for="c in cards"
          :key="c.id"
          :alarm="c"
          @click="goDetail"
        />
        <view v-if="loadingMore" class="loading-more">加载中…</view>
        <view v-else-if="!hasMore && cards.length" class="loading-more">没有更多了</view>
      </view>
    </state-view>
  </view>
</template>

<script setup>
/**
 * P8 告警历史查询（管理员）
 * 筛选：时间范围 + 教室 + 告警类型；筛选条件显示为可清除 tag
 */
import { ref, computed, reactive } from 'vue'
import { onShow, onPullDownRefresh, onReachBottom } from '@dcloudio/uni-app'
import { getAlarms } from '../../api'
import { store } from '../../store'
import { can, requireAuth } from '../../utils/permission'
import { toAlarmCard } from '../../utils/alarm'
import { COLORS } from '../../config/theme'

const TYPE_OPTIONS = [
  { key: 'sensor_threshold', label: '环境告警' },
  { key: 'device_offline', label: '设备离线' },
  { key: 'security', label: '安全告警' },
  { key: 'system', label: '系统事件' },
]
const typeKeys = TYPE_OPTIONS.map((t) => t.key)
const typeLabels = ['全部类型'].concat(TYPE_OPTIONS.map((t) => t.label))

const loading = ref(true)
const loadingMore = ref(false)
const loadError = ref('')
const permOk = ref(null)
const list = ref([])
const page = ref(1)
const hasMore = ref(false)

const filters = reactive({ start: '', end: '', space: '', type: '' })

const spaceNames = computed(() => ['全部教室'].concat(store.spaces.map((s) => s.name)))
const spaceLabel = computed(() => {
  const s = store.spaces.find((x) => x.space_id === filters.space)
  return s ? s.name : '全部教室'
})
const typeLabel = computed(() => {
  const t = TYPE_OPTIONS.find((x) => x.key === filters.type)
  return t ? t.label : '全部类型'
})

const activeTags = computed(() => {
  const tags = []
  if (filters.start) tags.push({ key: 'start', label: '起 ' + filters.start })
  if (filters.end) tags.push({ key: 'end', label: '止 ' + filters.end })
  if (filters.space) tags.push({ key: 'space', label: spaceLabel.value })
  if (filters.type) tags.push({ key: 'type', label: typeLabel.value })
  return tags
})

function setFilter(key, value) {
  filters[key] = value
  load(true)
}
function clearFilter(key) {
  filters[key] = ''
  load(true)
}
function clearAll() {
  filters.start = ''
  filters.end = ''
  filters.space = ''
  filters.type = ''
  load(true)
}

async function load(reset = false, append = false) {
  if (!requireAuth()) return
  permOk.value = can('alarm:history')
  if (!permOk.value) { loading.value = false; return }
  if (reset) { loading.value = true; page.value = 1 }
  loadError.value = ''

  const params = {
    page: append ? page.value + 1 : 1,
    page_size: 8,
  }
  if (filters.start) params.start = filters.start + 'T00:00:00Z'
  if (filters.end) params.end = filters.end + 'T23:59:59Z'
  if (filters.space) params.space_id = filters.space
  if (filters.type) params.type = filters.type

  const res = await getAlarms(params)
  if (res.error) {
    loadError.value = res.error.message
  } else {
    const rows = (res.data && res.data.list) || []
    list.value = append ? list.value.concat(rows) : rows
    hasMore.value = !!(res.data && res.data.has_more)
    if (append) page.value += 1
  }
  loading.value = false
  loadingMore.value = false
  uni.stopPullDownRefresh()
}

onShow(() => {
  if (!requireAuth()) return
  load(true)
})
onPullDownRefresh(() => load(true))
onReachBottom(() => {
  if (hasMore.value && !loadingMore.value) {
    loadingMore.value = true
    load(false, true)
  }
})

const cards = computed(() => list.value.map(toAlarmCard))

function goDetail(alarm) {
  uni.navigateTo({ url: '/pages/alarm/detail?id=' + alarm.id })
}

function goBack() {
  uni.navigateBack({ fail: () => uni.redirectTo({ url: '/pages/shell/shell' }) })
}
</script>

<style lang="scss" scoped>
.page { padding-bottom: 60rpx; }

.filter { padding-bottom: 24rpx; }
.frow { display: flex; align-items: center; gap: 14rpx; margin-bottom: 16rpx; }
.frow__dash { color: $text-3; font-size: $fs-sm; }
.fcell {
  flex: 1;
  background: $disabled-light;
  border-radius: $radius-sm;
  height: 80rpx;
  line-height: 80rpx;
  padding: 0 24rpx;
  font-size: $fs-sm;
  color: $text-1;
  text-align: center;
}
.fcell--full { width: 100%; }
.query-btn { width: 100%; margin-top: 8rpx; }

.tags-bar {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 12rpx;
  margin: 0 $gap 20rpx;
}
.ftag {
  display: inline-flex;
  align-items: center;
  background: $primary-light;
  color: $primary;
  border-radius: $r-tag;
  padding: 8rpx 22rpx;
  font-size: $fs-xs;
}
.ftag__x { margin-left: 10rpx; opacity: 0.7; }
.tags-bar__clear { color: $text-3; font-size: $fs-xs; padding: 0 10rpx; }

.list { padding: 0 $gap; }
.loading-more { text-align: center; color: $text-3; font-size: $fs-xs; padding: 24rpx 0; }
</style>
