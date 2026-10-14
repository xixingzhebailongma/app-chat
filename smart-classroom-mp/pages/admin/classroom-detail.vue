<template>
  <view class="page">
    <offline-banner :ws="true" />

    <state-view
      :loading="loading"
      :error="loadError"
      :perm="permOk === false ? false : null"
      perm-text="教室详情为管理员功能"
      empty-text="该教室暂无设备"
      empty-desc="设备接入网关后自动展示"
      empty-action="返回总览"
      empty-icon="room"
      @retry="load(true)"
      @perm-action="goBack"
      @empty-action="goBack"
    >
      <!-- 顶部（只读跨教室视角） -->
      <view class="head">
        <view class="head__name-row">
          <text class="head__name">{{ roomName }}</text>
          <view class="tag tag--blue">只读</view>
        </view>
        <view class="head__meta">
          <view class="head__meta-item">
            <app-icon name="sensor" :size="26" :color="COLORS.text3" />
            <text>{{ onlineCount }}/{{ devices.length }} 台在线</text>
          </view>
          <text v-if="lastSync">同步于 {{ lastSync }}</text>
        </view>
      </view>

      <!-- 环境数据 -->
      <view class="card">
        <view class="card__title">环境数据<text class="card__more">实时</text></view>
        <env-grid :devices="devices" />
        <view v-if="!readings.length" class="empty-line">暂无在线传感器数据</view>
      </view>

      <!-- 设备状态（在线/离线分组，只读） -->
      <view class="card">
        <view class="card__title">设备状态<text class="card__more">只读视图 · 控制请前往远程控制</text></view>
        <view v-for="d in onlineList" :key="d.device_id" class="dev-row">
          <view class="dot dot--green"></view>
          <text class="dev-row__name">{{ d.label || d.device_id }}</text>
          <text class="dev-row__state">{{ stateText(d) }}</text>
        </view>
        <view v-for="d in offlineList" :key="d.device_id" class="dev-row dev-row--off">
          <view class="dot dot--gray"></view>
          <text class="dev-row__name">{{ d.label || d.device_id }}</text>
          <text class="dev-row__state">离线</text>
        </view>
      </view>

      <!-- 今日课表 -->
      <view class="card">
        <view class="card__title">今日课表<text class="card__more">{{ weekdayText }}</text></view>
        <view v-for="e in todayEntries" :key="e.period" class="sch-row">
          <view class="sch-row__period">{{ e.period }}</view>
          <view class="sch-row__main">
            <text class="sch-row__subject">{{ e.subject }}</text>
            <text class="sch-row__teacher">{{ e.teacher }} · {{ e.start }}-{{ e.end }}</text>
          </view>
        </view>
        <view v-if="!todayEntries.length" class="empty-line">今日未排课（课表未配置或为空）</view>
      </view>

      <!-- 今日进出记录 -->
      <view class="card">
        <view class="card__title">今日进出<text class="card__more">{{ sessions.length }} 条</text></view>
        <view v-for="(s, i) in sessions.slice(0, 8)" :key="i" class="ses-row">
          <text class="ses-row__avatar">{{ (s.name || '?').slice(0, 1) }}</text>
          <view class="ses-row__main">
            <text class="ses-row__name">{{ s.name }}</text>
            <view class="tag" :class="s.auth_type === 'face' ? 'tag--blue' : 'tag--gray'">
              {{ s.auth_type === 'face' ? '人脸' : '刷卡' }}
            </view>
          </view>
          <text class="ses-row__time">{{ timeText(s.time) }}</text>
        </view>
        <view v-if="!sessions.length" class="empty-line">今日暂无进出记录</view>
      </view>
    </state-view>

    <!-- 底部：跳转远程控制 -->
    <view v-if="permOk && !loading && !loadError" class="action-bar">
      <button class="c-btn c-btn--primary" @tap="goRemote">对此教室远程控制</button>
    </view>
  </view>
</template>

<script setup>
/**
 * P5 教室详情（管理员）：结构参照 P1，只读跨教室视角，顶部带返回
 */
import { ref, computed } from 'vue'
import { onLoad, onShow, onPullDownRefresh, onUnload } from '@dcloudio/uni-app'
import { getDevices, getSchedules, getFaceSessions } from '../../api'
import { store, setDevices } from '../../store'
import { connectWs } from '../../services/ws'
import { can, requireAuth, gotoTab } from '../../utils/permission'
import { envReadings } from '../../utils/sensor'
import { fromNow, weekdayToday, WEEKDAY_NAMES } from '../../utils/time'
import { COLORS } from '../../config/theme'

const spaceId = ref('')
const roomName = ref('教室详情')
const loading = ref(true)
const loadError = ref('')
const permOk = ref(null)
const schedules = ref([])
const sessions = ref([])

const devices = computed(() => store.devices)
const readings = computed(() => envReadings(devices.value))
const onlineList = computed(() => devices.value.filter((d) => d.status === 'online'))
const offlineList = computed(() => devices.value.filter((d) => d.status !== 'online'))
const onlineCount = computed(() => onlineList.value.length)
const lastSync = computed(() => {
  void store.tick
  return store.devicesAt ? fromNow(new Date(store.devicesAt).toISOString()) : ''
})
const weekdayText = computed(() => WEEKDAY_NAMES[weekdayToday()])
const todayEntries = computed(() =>
  schedules.value.filter((e) => e.weekday === weekdayToday()).sort((a, b) => a.period - b.period)
)

function stateText(d) {
  if (d.app === 'on') return '开启中'
  if (d.app === 'off') return '已关闭'
  return '在线'
}
function timeText(t) {
  void store.tick
  return fromNow(t)
}

async function load(showLoading = false) {
  if (!requireAuth() || !spaceId.value) { loading.value = false; return }
  permOk.value = can('space:all')
  if (!permOk.value) { loading.value = false; return }
  if (showLoading) loading.value = true
  loadError.value = ''

  const devRes = await getDevices(spaceId.value)
  if (devRes.error) loadError.value = devRes.error.message
  else setDevices(devRes.data || [])

  const schRes = await getSchedules(spaceId.value)
  if (!schRes.error) schedules.value = (schRes.data && schRes.data.entries) || []

  const sesRes = await getFaceSessions({ space_id: spaceId.value })
  if (!sesRes.error) sessions.value = sesRes.data || []

  loading.value = false
  uni.stopPullDownRefresh()
}

onLoad((options) => {
  spaceId.value = (options && options.space_id) || ''
  roomName.value = (options && decodeURIComponent(options.name || '')) || '教室详情'
  uni.setNavigationBarTitle({ title: roomName.value })
})

onShow(() => {
  if (!requireAuth()) return
  if (!spaceId.value) return
  connectWs(spaceId.value) // 订阅该教室的 device_update
  load(true)
  uni.$off('ws:resync', onResync)
  uni.$on('ws:resync', onResync)
})
function onResync() { load() }
onPullDownRefresh(() => load())
onUnload(() => uni.$off('ws:resync', onResync))

function goBack() {
  uni.navigateBack({ fail: () => uni.redirectTo({ url: '/pages/shell/shell' }) })
}
function goRemote() {
  gotoTab('control', { space_id: spaceId.value })
}
</script>

<style lang="scss" scoped>
.page { padding-bottom: 200rpx; }

.head {
  margin: 20rpx $gap;
  background: $ink;
  border-radius: $radius;
  padding: 32rpx;
  color: $bg-card;
  box-shadow: $shadow-float;
}
.head__name-row { display: flex; align-items: center; gap: 14rpx; }
.head__name { font-size: $fs-xl; font-weight: 700; }
.head__meta { display: flex; gap: 30rpx; margin-top: 14rpx; font-size: $fs-xs; opacity: 0.8; }
.head__meta-item { display: flex; align-items: center; gap: 8rpx; }

.empty-line { text-align: center; color: $text-3; font-size: $fs-sm; padding: 26rpx 0 8rpx; }

.dev-row { display: flex; align-items: center; padding: 18rpx 0; border-bottom: 1rpx solid $border; }
.dev-row:last-child { border-bottom: none; }
.dev-row__name { flex: 1; margin-left: 16rpx; }
.dev-row__state { font-size: $fs-xs; color: $text-3; }
.dev-row--off { opacity: 0.55; }

.sch-row { display: flex; align-items: center; padding: 18rpx 0; border-bottom: 1rpx solid $border; }
.sch-row:last-child { border-bottom: none; }
.sch-row__period {
  width: 56rpx; height: 56rpx; border-radius: $r-icon;
  background: $primary-light; color: $primary;
  display: flex; align-items: center; justify-content: center;
  font-weight: 700; margin-right: 20rpx; flex-shrink: 0;
}
.sch-row__subject { font-size: $fs-md; font-weight: 600; }
.sch-row__teacher { display: block; font-size: $fs-xs; color: $text-3; margin-top: 4rpx; }

.ses-row { display: flex; align-items: center; padding: 16rpx 0; border-bottom: 1rpx solid $border; }
.ses-row:last-child { border-bottom: none; }
.ses-row__avatar {
  width: 56rpx; height: 56rpx; border-radius: 50%;
  background: $primary-light; color: $primary;
  display: flex; align-items: center; justify-content: center;
  font-weight: 600; margin-right: 18rpx;
}
.ses-row__main { flex: 1; display: flex; align-items: center; gap: 12rpx; }
.ses-row__time { font-size: $fs-xs; color: $text-3; }

.action-bar {
  position: fixed;
  left: $gap; right: $gap;
  bottom: calc(20rpx + constant(safe-area-inset-bottom));
  bottom: calc(20rpx + env(safe-area-inset-bottom));
  z-index: 997;
}
</style>
