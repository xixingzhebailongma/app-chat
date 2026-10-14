<template>
  <view class="ov">
    <offline-banner :ws="false" />

    <state-view
      :loading="loading"
      :error="loadError"
      :perm="permOk === false ? false : null"
      perm-text="全局总览为管理员功能"
      empty-text="暂无教室"
      empty-desc="教室接入云端后自动展示"
      empty-action="刷新"
      empty-icon="room"
      @retry="load(true)"
      @perm-action="goTeacherHome"
      @empty-action="load(true)"
    >
      <!-- 顶部浅色 Hero：全局状态总览（品牌浅蓝渐变 + 彩色图标） -->
      <view class="ov__hero">
        <view class="ov__hero-glow ov__hero-glow--a" />
        <view class="ov__hero-glow ov__hero-glow--b" />
        <view class="ov__hero-head">
          <view class="ov__hero-badge">
            <app-icon name="grid" :size="34" color="#FFFFFF" />
          </view>
          <view class="ov__hero-title-text">
            <text class="ov__hero-name">智慧教室总览</text>
            <text class="ov__hero-sub">{{ totals.devices_online }}/{{ totals.devices_total }} 台设备在线 · 在线率 {{ totalRate }}%</text>
          </view>
        </view>
        <view class="ov__hero-stats">
          <view class="ov__hero-item">
            <view class="ov__hero-ic ov__hero-ic--blue">
              <app-icon name="room" :size="34" color="#FFFFFF" />
            </view>
            <text class="ov__hero-num num">{{ totals.spaces }}</text>
            <text class="ov__hero-label">教室总数</text>
          </view>
          <view class="ov__hero-item">
            <view class="ov__hero-ic ov__hero-ic--green">
              <app-icon name="wifi" :size="34" color="#FFFFFF" />
            </view>
            <text class="ov__hero-num num">{{ totals.devices_online }}</text>
            <text class="ov__hero-label">在线设备</text>
          </view>
          <view class="ov__hero-item">
            <view class="ov__hero-ic ov__hero-ic--red">
              <app-icon name="warning" :size="34" color="#FFFFFF" />
            </view>
            <text class="ov__hero-num num" :class="{ 'ov__hero-num--danger': totals.pending_alarms > 0 }">{{ totals.pending_alarms }}</text>
            <text class="ov__hero-label">未处理告警</text>
          </view>
        </view>
      </view>

      <!-- 快捷筛选：全部 / 有告警 -->
      <view class="ov__chips">
        <view class="ov__chip" :class="{ 'ov__chip--on': filter === 'all' }" hover-class="press" @tap="filter = 'all'">全部</view>
        <view class="ov__chip" :class="{ 'ov__chip--on': filter === 'alarm' }" hover-class="press" @tap="filter = 'alarm'">
          有告警 <text v-if="alarmSpaceCount">({{ alarmSpaceCount }})</text>
        </view>
        <view class="ov__chip ov__chip--tool" hover-class="press" @tap="goHistory">
          <app-icon name="clock" :size="26" :color="COLORS.primary" />
          <text>历史查询</text>
        </view>
      </view>

      <!-- 教室卡片：有未处理告警 → 整卡警示边框（不只靠小红点） -->
      <view class="ov__rooms">
        <view
          v-for="r in shownRooms"
          :key="r.space_id"
          class="ov__room"
          :class="{ 'ov__room--alarm': r.pending_alarms > 0 }"
          hover-class="ov__room--press"
          :hover-stay-time="60"
          @tap="goRoom(r)"
        >
          <view class="ov__room-head">
            <view class="ov__room-name-row">
              <view class="ov__room-icon">
                <app-icon name="room" :size="34" :color="COLORS.primary" />
              </view>
              <text class="ov__room-name">{{ r.name }}</text>
            </view>
            <view v-if="r.pending_alarms > 0" class="tag tag--red">{{ r.pending_alarms }} 条未处理</view>
            <view v-else class="tag tag--green">正常</view>
          </view>
          <view class="ov__room-body">
            <view class="ov__rate">
              <view class="ov__bar">
                <view class="ov__bar-in" :style="{ width: rate(r) + '%' }" />
              </view>
              <text class="ov__rate-text">设备在线率 {{ rate(r) }}%</text>
            </view>
            <text class="ov__room-count num">{{ r.device_online }}/{{ r.device_total }} 台</text>
          </view>
        </view>
      </view>
    </state-view>

    <view class="safe-bottom" />
  </view>
</template>

<script setup>
/**
 * P4 全局总览（管理员首页）· 壳层视图
 * - 汇总：教室数 / 在线设备 / 未处理告警（异常醒目色）
 * - 教室卡片：有未处理告警 → 整卡警示边框
 */
import { ref, computed } from 'vue'
import { getOverview } from '../../api'
import { store } from '../../store'
import { can, requireAuth, gotoTab } from '../../utils/permission'
import { COLORS } from '../../config/theme'

const loading = ref(true)
const loadError = ref('')
const permOk = ref(null)
const inited = ref(false)
const rooms = ref([])
const filter = ref('all')
const rawTotals = ref(null)

const totals = computed(() => ({
  spaces: 0, devices_online: 0, devices_total: 0, pending_alarms: 0,
  ...(rawTotals.value || {}),
}))

const alarmSpaceCount = computed(() => rooms.value.filter((r) => r.pending_alarms > 0).length)
const totalRate = computed(() => {
  if (!totals.value.devices_total) return 0
  return Math.round((totals.value.devices_online / totals.value.devices_total) * 100)
})
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
  inited.value = true
  uni.stopPullDownRefresh()
}

/* 壳层协议 */
function onEnter() {
  if (!requireAuth()) return
  load(!inited.value)
}
function onRefresh() { load() }

function goRoom(r) {
  uni.navigateTo({
    url: '/pages/admin/classroom-detail?space_id=' + r.space_id + '&name=' + encodeURIComponent(r.name),
  })
}
function goHistory() {
  uni.navigateTo({ url: '/pages/admin/history' })
}
function goTeacherHome() {
  gotoTab('classroom')
}

defineExpose({ onEnter, onRefresh })
</script>

<style lang="scss" scoped>
.ov { padding-bottom: $sp-16; }

/* 浅色 Hero：全局状态总览，品牌浅蓝渐变底，清爽不死黑 */
.ov__hero {
  position: relative;
  overflow: hidden;
  margin: $sp-20 $page-pad $sp-24;
  background: linear-gradient(135deg, #F8FAFF 0%, #EBF3FF 100%);
  border-radius: $r-card;
  padding: $card-pad;
  color: $text-1;
  box-shadow: 0 8rpx 28rpx rgba(0, 122, 255, 0.08);
  border: 1rpx solid rgba(0, 122, 255, 0.12);
}
/* 顶部装饰光斑：柔和的浅蓝+浅紫，增加活力 */
.ov__hero-glow {
  position: absolute;
  border-radius: 50%;
  filter: blur(14rpx);
  pointer-events: none;
}
.ov__hero-glow--a {
  top: -90rpx;
  right: -60rpx;
  width: 240rpx;
  height: 240rpx;
  background: radial-gradient(circle, rgba(74, 158, 255, 0.24), transparent 70%);
}
.ov__hero-glow--b {
  bottom: -100rpx;
  left: -40rpx;
  width: 220rpx;
  height: 220rpx;
  background: radial-gradient(circle, rgba(175, 82, 222, 0.16), transparent 70%);
}
.ov__hero-head {
  position: relative;
  @include flex(flex-start, center);
  gap: $sp-16;
  margin-bottom: $sp-28;
}
.ov__hero-badge {
  width: 72rpx;
  height: 72rpx;
  border-radius: 22rpx;
  background: linear-gradient(135deg, #4A9EFF 0%, #007AFF 100%);
  box-shadow: 0 8rpx 22rpx rgba(0, 122, 255, 0.35);
  @include flex(center, center);
  flex: none;
}
.ov__hero-title-text { min-width: 0; }
.ov__hero-name {
  display: block;
  font-size: $fs-page-title;
  font-weight: 700;
  color: $text-1;
}
.ov__hero-sub {
  display: block;
  font-size: $fs-aux;
  color: $text-2;
  margin-top: $sp-8;
}
.ov__hero-stats {
  position: relative;
  @include flex(center, center);
  background: #fff;
  border-radius: $r-card;
  padding: $sp-28 0;
  box-shadow: 0 4rpx 16rpx rgba(0, 0, 0, 0.04);
}
.ov__hero-item {
  flex: 1;
  @include flex(center, center, column);
  position: relative;
}
.ov__hero-item:not(:last-child)::after {
  content: '';
  position: absolute;
  right: 0;
  top: 50%;
  transform: translateY(-50%);
  width: 1rpx;
  height: 56rpx;
  background: $line;
}
/* 彩色柔光图标圆底：每个指标一种语义色，带同色光晕 */
.ov__hero-ic {
  width: 64rpx;
  height: 64rpx;
  border-radius: 50%;
  @include flex(center, center);
  margin-bottom: $sp-16;
}
.ov__hero-ic--blue  { background: linear-gradient(135deg, #4A9EFF 0%, #007AFF 100%); box-shadow: 0 8rpx 20rpx rgba(0, 122, 255, 0.35); }
.ov__hero-ic--green { background: linear-gradient(135deg, #5BD08A 0%, #34C759 100%); box-shadow: 0 8rpx 20rpx rgba(52, 199, 89, 0.32); }
.ov__hero-ic--red   { background: linear-gradient(135deg, #FF7A6E 0%, #FF3B30 100%); box-shadow: 0 8rpx 20rpx rgba(255, 59, 48, 0.32); }
.ov__hero-num { font-size: 56rpx; font-weight: 700; line-height: 72rpx; color: $text-1; }
.ov__hero-num--danger { color: $danger; }
.ov__hero-label { font-size: $fs-tag; color: $text-3; margin-top: 6rpx; }

.ov__chips {
  @include flex(flex-start, center);
  gap: $sp-16;
  margin: 0 $page-pad $sp-20;
}
.ov__chip {
  padding: 12rpx 30rpx;
  border-radius: $r-tag;
  background: $bg-card;
  box-shadow: $shadow-card;
  font-size: $fs-aux;
  color: $text-2;
  @include flex(flex-start, center);
  gap: 6rpx;
}
.ov__chip--on { background: $primary; color: $bg-card; font-weight: 600; box-shadow: $shadow-main-btn; }
.ov__chip--tool { margin-left: auto; color: $primary; }

.ov__rooms { padding: 0 $page-pad; }
.ov__room {
  background: $bg-card;
  border-radius: $r-card;
  padding: 26rpx;
  margin-bottom: $sp-16;
  box-shadow: $shadow-card;
  transition: transform $t-micro $ease-standard;
}
.ov__room--press { transform: scale(0.98); }
.ov__room--alarm {
  box-shadow: $shadow-card, $ring-danger;
}
.ov__room-head {
  @include flex(space-between, center);
}
.ov__room-name-row { @include flex(flex-start, center); gap: $sp-12; min-width: 0; }
.ov__room-icon {
  width: 56rpx;
  height: 56rpx;
  border-radius: $r-icon;
  background: $primary-light;
  @include flex(center, center);
  flex: none;
}
.ov__room-name { font-size: $fs-card-title; font-weight: 600; color: $text-1; }
.ov__room-body {
  @include flex(flex-start, center);
  gap: $sp-20;
  margin-top: $sp-20;
}
.ov__rate { flex: 1; }
.ov__bar {
  height: 14rpx;
  border-radius: 8rpx;
  background: $disabled-light;
  overflow: hidden;
}
.ov__bar-in {
  height: 100%;
  border-radius: 8rpx;
  background: $success;
  transition: width 0.4s;
}
.ov__rate-text { display: block; font-size: $fs-tag; color: $text-3; margin-top: 10rpx; }
.ov__room-count { font-size: $fs-aux; color: $text-2; }
</style>
