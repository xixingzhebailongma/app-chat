<template>
  <view class="page">
    <OfflineBanner :ws="false" />

    <StateView
      :loading="loading"
      :error="loadError"
      :perm="permOk === false ? false : null"
      perm-text="全局总览为管理员功能"
      empty-text="暂无教室"
      empty-desc="教室接入云端后自动展示"
      empty-action="刷新"
      empty-icon="🏫"
      @retry="load(true)"
      @perm-action="goTeacherHome"
      @empty-action="load(true)"
    >
      <!-- 顶部汇总条 -->
      <view class="sum">
        <view class="sum__item">
          <text class="sum__num">{{ totals.spaces }}</text>
          <text class="sum__label">教室总数</text>
        </view>
        <view class="sum__divider"></view>
        <view class="sum__item">
          <text class="sum__num">{{ totals.devices_online }}<text class="sum__sub">/{{ totals.devices_total }}</text></text>
          <text class="sum__label">在线设备</text>
        </view>
        <view class="sum__divider"></view>
        <view class="sum__item">
          <text class="sum__num" :class="{ 'sum__num--danger': totals.pending_alarms > 0 }">{{ totals.pending_alarms }}</text>
          <text class="sum__label">未处理告警</text>
        </view>
      </view>

      <!-- 快捷筛选：全部 / 有告警 -->
      <view class="chips">
        <view class="chip" :class="{ 'chip--on': filter === 'all' }" @tap="filter = 'all'">全部</view>
        <view class="chip" :class="{ 'chip--on': filter === 'alarm' }" @tap="filter = 'alarm'">
          有告警 <text v-if="alarmSpaceCount">({{ alarmSpaceCount }})</text>
        </view>
        <view class="chip chip--tool" @tap="goHistory">📅 历史查询</view>
      </view>

      <!-- 教室卡片：有未处理告警 → 整卡警示边框 -->
      <view class="rooms">
        <view
          v-for="r in shownRooms"
          :key="r.space_id"
          class="room"
          :class="{ 'room--alarm': r.pending_alarms > 0 }"
          @tap="goRoom(r)"
        >
          <view class="room__head">
            <text class="room__name">{{ r.name }}</text>
            <view v-if="r.pending_alarms > 0" class="tag tag--red">{{ r.pending_alarms }} 条未处理</view>
            <view v-else class="tag tag--green">正常</view>
          </view>
          <view class="room__body">
            <view class="room__rate">
              <view class="room__bar">
                <view class="room__bar-in" :style="{ width: rate(r) + '%' }"></view>
              </view>
              <text class="room__rate-text">设备在线率 {{ rate(r) }}%</text>
            </view>
            <text class="room__count">{{ r.device_online }}/{{ r.device_total }} 台</text>
          </view>
        </view>
      </view>
    </StateView>

    <view class="safe-bottom"></view>
    <TabBar current="/pages/admin/overview" />
  </view>
</template>

<script setup>
/**
 * P4 全局总览（管理员首页）
 * - 汇总：教室数 / 在线设备 / 未处理告警（异常醒目色）
 * - 教室卡片：有未处理告警 → 整卡警示边框（不只靠小红点）
 */
import { ref, computed } from 'vue'
import { onShow, onPullDownRefresh } from '@dcloudio/uni-app'
import { getOverview } from '../../api'
import { store } from '../../store'
import { can, requireAuth } from '../../utils/permission'
import OfflineBanner from '../../components/OfflineBanner.vue'
import StateView from '../../components/StateView.vue'
import TabBar from '../../components/TabBar.vue'

const loading = ref(true)
const loadError = ref('')
const permOk = ref(null)
const rooms = ref([])
const filter = ref('all')

const totals = computed(() => ({
  spaces: 0, devices_online: 0, devices_total: 0, pending_alarms: 0,
  ...(rawTotals.value || {}),
}))
const rawTotals = ref(null)

const alarmSpaceCount = computed(() => rooms.value.filter((r) => r.pending_alarms > 0).length)
const shownRooms = computed(() => {
  const list = rooms.value.slice().sort((a, b) => b.pending_alarms - a.pending_alarms)
  return filter.value === 'alarm' ? list.filter((r) => r.pending_alarms > 0) : list
})

function rate(r) {
  if (!r.device_total) return 0
  return Math.round((r.device_online / r.device_total) * 100)
}

async function load(showLoading = false) {
  if (!requireAuth()) return
  permOk.value = can('space:all')
  if (!permOk.value) { loading.value = false; return }
  if (showLoading) loading.value = true
  loadError.value = ''
  const res = await getOverview()
  if (res.error) {
    loadError.value = res.error.message
  } else {
    rooms.value = (res.data && res.data.classrooms) || []
    rawTotals.value = (res.data && res.data.totals) || null
    store.pendingAlarmCount = (rawTotals.value && rawTotals.value.pending_alarms) || 0
  }
  loading.value = false
  uni.stopPullDownRefresh()
}

onShow(() => {
  if (!requireAuth()) return
  load(true)
})
onPullDownRefresh(() => load())

function goRoom(r) {
  uni.navigateTo({
    url: '/pages/admin/classroom-detail?space_id=' + r.space_id + '&name=' + encodeURIComponent(r.name),
  })
}
function goHistory() {
  uni.navigateTo({ url: '/pages/admin/history' })
}
function goTeacherHome() {
  uni.redirectTo({ url: '/pages/teacher/classroom' })
}
</script>

<style lang="scss" scoped>
.page { padding-bottom: 40rpx; }

.sum {
  margin: 20rpx $gap;
  background: linear-gradient(135deg, #6B93FF, $primary);
  border-radius: $radius;
  padding: 34rpx 20rpx;
  display: flex;
  align-items: center;
  color: #fff;
  box-shadow: 0 8rpx 24rpx rgba(79, 124, 255, 0.25);
}
.sum__item { flex: 1; display: flex; flex-direction: column; align-items: center; }
.sum__num { font-size: 52rpx; font-weight: 700; line-height: 68rpx; }
.sum__num--danger { color: #FFD9D9; text-shadow: 0 0 20rpx rgba(255, 255, 255, 0.6); }
.sum__sub { font-size: $fs-sm; font-weight: 400; opacity: 0.8; }
.sum__label { font-size: $fs-xs; opacity: 0.85; margin-top: 4rpx; }
.sum__divider { width: 1rpx; height: 60rpx; background: rgba(255, 255, 255, 0.25); }

.chips { display: flex; gap: 16rpx; margin: 0 $gap 20rpx; }
.chip {
  padding: 10rpx 30rpx;
  border-radius: 40rpx;
  background: #fff;
  border: 2rpx solid $border;
  font-size: $fs-sm;
  color: $text-2;
}
.chip--on { background: $primary; border-color: $primary; color: #fff; font-weight: 600; }
.chip--tool { margin-left: auto; color: $primary; border-color: rgba(79, 124, 255, 0.35); }

.rooms { padding: 0 $gap; }
.room {
  background: $card;
  border-radius: $radius-sm;
  padding: 26rpx;
  margin-bottom: 16rpx;
  border: 3rpx solid transparent;
  box-shadow: $shadow;
}
.room--alarm { border-color: rgba(239, 68, 68, 0.55); background: #FFFAFA; }
.room__head { display: flex; align-items: center; justify-content: space-between; }
.room__name { font-size: $fs-lg; font-weight: 600; }
.room__body { display: flex; align-items: center; gap: 20rpx; margin-top: 22rpx; }
.room__rate { flex: 1; }
.room__bar { height: 14rpx; border-radius: 8rpx; background: #EDF0F5; overflow: hidden; }
.room__bar-in { height: 100%; border-radius: 8rpx; background: linear-gradient(90deg, #34D399, $success); transition: width 0.4s; }
.room__rate-text { display: block; font-size: $fs-xs; color: $text-3; margin-top: 10rpx; }
.room__count { font-size: $fs-sm; color: $text-2; }
</style>
