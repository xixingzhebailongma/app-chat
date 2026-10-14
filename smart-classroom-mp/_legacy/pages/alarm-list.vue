<template>
  <view class="page">
    <OfflineBanner :ws="false" />

    <!-- 推送分级说明：解释“小程序里没看到告警”≠“没发生” -->
    <view v-if="showPushTip" class="push-tip">
      <text class="push-tip__icon">💡</text>
      <text class="push-tip__text">超阈值需持续 5 分钟才推送；同类告警 30 分钟冷却期内合并；主渠道失败会降级短信。未收到推送 ≠ 没有发生。</text>
      <text class="push-tip__close" @tap="showPushTip = false">✕</text>
    </view>

    <!-- 状态分组：教师 2 态 / 管理员 3 态 -->
    <view class="seg-wrap">
      <view class="seg">
        <view
          v-for="f in filters"
          :key="f.key"
          class="seg__item"
          :class="{ 'seg__item--on': active === f.key }"
          @tap="switchFilter(f.key)"
        >{{ f.label }}</view>
      </view>
    </view>

    <StateView
      :loading="loading"
      :error="loadError"
      :empty="!loading && !loadError && !list.length"
      :perm="permOk === false ? false : null"
      :empty-text="emptyText"
      :empty-desc="emptyDesc"
      :empty-action="emptyActionText"
      empty-icon="🔔"
      @retry="load(true)"
      @perm-action="goHome"
      @empty-action="goHome"
    >
      <view class="list">
        <view v-for="a in list" :key="a.id">
          <AlarmCard :alarm="a" />
        </view>
        <view v-if="loadingMore" class="loading-more">加载中…</view>
        <view v-else-if="!hasMore && list.length" class="loading-more">没有更多了</view>
      </view>
    </StateView>

    <view class="safe-bottom"></view>
    <TabBar current="/pages/alarm/list" />
  </view>
</template>

<script setup>
/**
 * P3 告警列表（教师）/ P6 告警处理（管理员）—— 同页角色隔离：
 * - 教师：未处理 / 已处理 两组，没有「标记已处理」按钮
 * - 管理员：未处理 / 处理中 / 已解决 三态筛选
 * - 未处理置顶并用色点/整卡强调；每条标注被抑制/降级原因
 */
import { ref, computed } from 'vue'
import { onShow, onPullDownRefresh, onReachBottom, onUnload } from '@dcloudio/uni-app'
import { store } from '../../store'
import { getAlarms } from '../../api'
import { isAdmin, isTeacher, can, requireAuth } from '../../utils/permission'
import OfflineBanner from '../../components/OfflineBanner.vue'
import StateView from '../../components/StateView.vue'
import AlarmCard from '../../components/AlarmCard.vue'
import TabBar from '../../components/TabBar.vue'

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
const list = ref([])
const page = ref(1)
const hasMore = ref(false)
const active = ref('pending')

const filters = computed(() => (isAdmin() ? ADMIN_FILTERS : TEACHER_FILTERS))
const emptyText = computed(() => (active.value === 'pending' ? '暂无未处理告警' : '暂无相关告警'))
const emptyDesc = computed(() => '教室运行正常，异常事件将第一时间推送')
const emptyActionText = computed(() => (isAdmin() ? '返回总览' : '返回我的教室'))

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

function onRefresh() { load(true); loadBadge() }

onShow(() => {
  if (!requireAuth()) return
  load(true)
  loadBadge()
  // 详情页处理完告警后刷新
  uni.$off('alarm:changed', onRefresh)
  uni.$on('alarm:changed', onRefresh)
})

onPullDownRefresh(() => onRefresh())
onReachBottom(() => {
  if (hasMore.value && !loadingMore.value) {
    loadingMore.value = true
    load(false, true)
  }
})
onUnload(() => uni.$off('alarm:changed', onRefresh))

function goHome() {
  if (isAdmin()) uni.redirectTo({ url: '/pages/admin/overview' })
  else uni.redirectTo({ url: '/pages/teacher/classroom' })
}
</script>

<style lang="scss" scoped>
.page { padding-bottom: 40rpx; }

.push-tip {
  display: flex;
  align-items: flex-start;
  margin: 20rpx $gap 0;
  background: #F0F6FF;
  border-radius: $radius-sm;
  padding: 18rpx 22rpx;
  font-size: $fs-xs;
  color: $text-2;
  line-height: 36rpx;
}
.push-tip__icon { margin-right: 12rpx; }
.push-tip__text { flex: 1; }
.push-tip__close { color: $text-3; padding: 0 6rpx; }

.seg-wrap { margin: 20rpx $gap 16rpx; }
.list { padding: 0 $gap; }
.loading-more { text-align: center; color: $text-3; font-size: $fs-xs; padding: 24rpx 0 40rpx; }
</style>
