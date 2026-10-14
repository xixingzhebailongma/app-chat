<template>
  <view class="av">
    <offline-banner :ws="false" />

    <!-- 推送分级说明：解释"小程序里没看到告警"≠"没发生" -->
    <view v-if="showPushTip" class="av__push-tip">
      <view class="av__push-icon">
        <app-icon name="info" :size="30" :color="COLORS.primary" />
      </view>
      <text class="av__push-text">超阈值需持续 5 分钟才推送；同类告警 30 分钟冷却期内合并；主渠道失败会降级短信。未收到推送 ≠ 没有发生。</text>
      <text class="av__push-close" hover-class="press" @tap="dismissPushTip">
        <app-icon name="close" :size="26" :color="COLORS.text3" />
      </text>
    </view>

    <!-- 状态分组：教师 2 态 / 管理员 3 态 -->
    <view class="av__seg-wrap">
      <view class="seg">
        <view
          v-for="f in filters"
          :key="f.key"
          class="seg__item"
          :class="{ 'seg__item--on': active === f.key }"
          hover-class="press"
          @tap="switchFilter(f.key)"
        >{{ f.label }}</view>
      </view>
    </view>

    <state-view
      :loading="loading"
      :error="loadError"
      :empty="!loading && !loadError && !cards.length"
      :perm="permOk === false ? false : null"
      :empty-text="emptyText"
      :empty-desc="emptyDesc"
      :empty-action="emptyActionText"
      empty-icon="bell"
      @retry="load(true)"
      @perm-action="goHome"
      @empty-action="goHome"
    >
      <view class="av__list">
        <alarm-card
          v-for="c in cards"
          :key="c.id"
          :alarm="c"
          @click="goDetail"
        />
        <view v-if="loadingMore" class="av__more">加载中…</view>
        <view v-else-if="!hasMore && cards.length" class="av__more">没有更多了</view>
      </view>
    </state-view>

    <view class="safe-bottom" />
  </view>
</template>

<script setup>
/**
 * P3 告警列表（教师）/ P6 告警处理（管理员）· 壳层视图，同视图角色隔离：
 * - 教师：未处理 / 已处理 两组，没有「标记已处理」按钮（详情页同样按角色隐藏）
 * - 管理员：未处理 / 处理中 / 已解决 三态筛选
 * - 未处理置顶（色条 + 光晕强调）；每条标注被抑制/降级原因
 */
import { ref, computed, onMounted, onUnmounted } from 'vue'
import { store } from '../../store'
import { getAlarms } from '../../api'
import { isAdmin, isTeacher, can, requireAuth, gotoTab } from '../../utils/permission'
import { toAlarmCard } from '../../utils/alarm'
import { COLORS } from '../../config/theme'

const TEACHER_FILTERS = [
  { key: 'pending', label: '未处理', status: 'pending' },
  { key: 'done', label: '已处理', status: 'processing,resolved' },
]
const ADMIN_FILTERS = [
  { key: 'pending', label: '未处理', status: 'pending' },
  { key: 'processing', label: '处理中', status: 'processing' },
  { key: 'resolved', label: '已解决', status: 'resolved' },
]

const showPushTip = ref(uni.getStorageSync('sc_push_tip_dismissed') !== 'yes')
const loading = ref(true)
const loadingMore = ref(false)
const loadError = ref('')
const permOk = ref(null)
const inited = ref(false)
const list = ref([])
const page = ref(1)
const hasMore = ref(false)
const active = ref('pending')

const filters = computed(() => (isAdmin() ? ADMIN_FILTERS : TEACHER_FILTERS))
const cards = computed(() => list.value.map(toAlarmCard))
const emptyText = computed(() => (active.value === 'pending' ? '暂无未处理告警' : '暂无相关告警'))
const emptyDesc = computed(() => '教室运行正常，异常事件将第一时间推送')
const emptyActionText = computed(() => (isAdmin() ? '返回总览' : '返回我的教室'))

function dismissPushTip() {
  showPushTip.value = false
  uni.setStorageSync('sc_push_tip_dismissed', 'yes')
}

function currentStatus() {
  const f = filters.value.find((x) => x.key === active.value)
  return f ? f.status : 'pending'
}

async function load(reset = false, append = false) {
  if (!requireAuth()) return
  permOk.value = can('alarm:view')
  if (!permOk.value) { loading.value = false; return }

  if (reset) {
    loading.value = true
    page.value = 1
  }
  loadError.value = ''

  const params = {
    status: currentStatus(),
    page: append ? page.value + 1 : 1,
    page_size: 8,
  }
  // 教师只看本教室告警（云端同样做角色隔离，双保险）
  if (isTeacher()) params.space_id = store.spaceId

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
  inited.value = true
  uni.stopPullDownRefresh()
}

async function loadBadge() {
  const params = { status: 'pending', page: 1, page_size: 1 }
  if (isTeacher()) params.space_id = store.spaceId
  const res = await getAlarms(params)
  if (!res.error && res.data) store.pendingAlarmCount = res.data.total || 0
}

function switchFilter(key) {
  if (active.value === key) return
  active.value = key
  load(true)
}

function onRefreshAll() { load(true); loadBadge() }

/* 壳层协议 */
function onEnter() {
  if (!requireAuth()) return
  load(true)
  loadBadge()
}
function onRefresh() { onRefreshAll() }
function onLoadMore() {
  if (hasMore.value && !loadingMore.value) {
    loadingMore.value = true
    load(false, true)
  }
}

/* 详情页处理完告警后刷新（壳层常驻，仅注册一次） */
onMounted(() => uni.$on('alarm:changed', onRefreshAll))
onUnmounted(() => uni.$off('alarm:changed', onRefreshAll))

function goDetail(alarm) {
  uni.navigateTo({ url: '/pages/alarm/detail?id=' + alarm.id })
}
function goHome() {
  gotoTab(isAdmin() ? 'overview' : 'classroom')
}

defineExpose({ onEnter, onRefresh, onLoadMore })
</script>

<style lang="scss" scoped>
.av { padding-bottom: $sp-16; }

.av__push-tip {
  @include flex(flex-start, flex-start);
  margin: $sp-16 $page-pad 0;
  background: $primary-light;
  border-radius: $r-icon;
  padding: 18rpx 22rpx;
  font-size: $fs-tag;
  color: $text-2;
  line-height: 36rpx;
}
.av__push-icon { margin-right: $sp-12; margin-top: 2rpx; }
.av__push-text { flex: 1; }
.av__push-close { padding: 0 6rpx; }

.av__seg-wrap { margin: $sp-20 $page-pad $sp-16; }
.av__list { padding: 0 $page-pad; }
.av__more {
  text-align: center;
  color: $text-3;
  font-size: $fs-tag;
  padding: $sp-24 0 40rpx;
}
</style>
