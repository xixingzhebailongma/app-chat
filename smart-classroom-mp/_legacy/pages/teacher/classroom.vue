<template>
  <view class="page">
    <OfflineBanner :ws="true" />

    <StateView
      :loading="loading"
      :error="loadError"
      :empty="noClassroom"
      :perm="permOk === false ? false : null"
      perm-text="此页面为教师端首页，当前账号无权访问"
      perm-action="返回"
      empty-text="还未绑定教室"
      empty-desc="请联系管理员在后台为您绑定教室后使用"
      empty-action="联系管理员说明"
      empty-icon="🏫"
      @retry="refreshAll(true)"
      @perm-action="goHome"
      @empty-action="goProfile"
    >
      <!-- 顶部：教室名 + 当前时间 + 连接状态 -->
      <view class="head">
        <view class="head__left">
          <view class="head__name-row">
            <text class="head__name">{{ classroomName }}</text>
            <view class="tag tag--blue">{{ roleName }}</view>
          </view>
          <text class="head__time">{{ clock }} · {{ weekdayText }}</text>
        </view>
        <view class="head__ws">
          <view class="dot" :class="wsDotClass"></view>
          <text class="head__ws-text">{{ wsText }}</text>
        </view>
      </view>

      <!-- 订阅引导条（轻量解释价值，用户点击后才调起授权，不自动弹） -->
      <view v-if="showSubGuide" class="subguide">
        <text class="subguide__icon">🔔</text>
        <text class="subguide__text">开启告警推送，教室异常第一时间通知您</text>
        <text class="subguide__btn" @tap="requestSub">去开启</text>
        <text class="subguide__close" @tap="dismissSubGuide">✕</text>
      </view>

      <!-- 环境数据卡片区：只渲染实际存在的指标 -->
      <view class="card">
        <view class="card__title">环境数据<text class="card__more">实时</text></view>
        <SensorPanel :devices="devices" />
        <view v-if="!readings.length" class="env-empty">教室内暂无在线传感器</view>
      </view>

      <!-- 设备状态：在线 / 离线分组，离线置灰置底 -->
      <view class="card">
        <view class="card__title">设备状态<text class="card__more">{{ onlineList.length }} 在线 / {{ offlineList.length }} 离线</text></view>
        <view v-for="d in onlineList" :key="d.device_id" class="dev-row">
          <view class="dot dot--green"></view>
          <text class="dev-row__name">{{ d.label || d.device_id }}</text>
          <text class="dev-row__time">{{ lastSeenText(d) }}</text>
        </view>
        <view v-for="d in offlineList" :key="d.device_id" class="dev-row dev-row--off">
          <view class="dot dot--gray"></view>
          <text class="dev-row__name">{{ d.label || d.device_id }}</text>
          <text class="dev-row__time">{{ lastSeenText(d) }}</text>
        </view>
      </view>

      <!-- 今日进出记录 -->
      <view class="card">
        <view class="card__title">
          今日进出
          <text class="card__more" @tap="sessionsExpanded = !sessionsExpanded">
            {{ sessionsExpanded ? '收起' : '查看全部' }}
          </text>
        </view>
        <view v-for="(s, i) in shownSessions" :key="i" class="ses-row">
          <text class="ses-row__avatar">{{ (s.name || '?').slice(0, 1) }}</text>
          <view class="ses-row__main">
            <text class="ses-row__name">{{ s.name }}</text>
            <view class="tag" :class="s.auth_type === 'face' ? 'tag--blue' : 'tag--gray'">
              {{ s.auth_type === 'face' ? '人脸' : '刷卡' }}
            </view>
          </view>
          <view class="ses-row__side">
            <text class="ses-row__time">{{ timeText(s.time) }}</text>
            <view v-if="s.result === 'denied'" class="tag tag--red">拒绝</view>
          </view>
        </view>
        <view v-if="!sessions.length" class="env-empty">今日暂无进出记录</view>
      </view>
    </StateView>

    <!-- 底部固定操作栏：场景快捷控制 -->
    <view class="bottom-bar" v-if="permOk !== false && !loading && !loadError">
      <SceneBar @scene="onScene" />
    </view>
    <view class="safe-bottom"></view>

    <TabBar current="/pages/teacher/classroom" />
  </view>
</template>

<script setup>
/**
 * P1 我的教室（教师首页）：进入直接呈现绑定教室实时状态
 */
import { ref, computed, onUnmounted } from 'vue'
import { onShow, onPullDownRefresh } from '@dcloudio/uni-app'
import { store } from '../../store'
import { getDevices, getFaceSessions, getAlarms } from '../../api'
import { SUBSCRIBE_TMPL_IDS } from '../../api/config'
import { setDevices, setSubscribed } from '../../store'
import { connectWs } from '../../services/ws'
import { can, isAdmin, requireAuth } from '../../utils/permission'
import { fromNow, fmtClockSec, weekdayToday, WEEKDAY_NAMES } from '../../utils/time'
import { runScene } from '../../utils/scene'
import SensorPanel from '../../components/SensorPanel.vue'
import OfflineBanner from '../../components/OfflineBanner.vue'
import StateView from '../../components/StateView.vue'
import SceneBar from '../../components/SceneBar.vue'
import TabBar from '../../components/TabBar.vue'
import { envReadings } from '../../utils/sensor'

const loading = ref(true)
const loadError = ref('')
const permOk = ref(null)
const sessions = ref([])
const sessionsExpanded = ref(false)
const showSubGuide = ref(
  uni.getStorageSync('sc_sub_guide_dismissed') !== 'yes' && !store.subscribed
)

/* 实时时钟 */
const now = ref(Date.now())
const clockTimer = setInterval(() => { now.value = Date.now() }, 1000)
const clock = computed(() => fmtClockSec(new Date(now.value)))
const weekdayText = computed(() => WEEKDAY_NAMES[weekdayToday()])

const devices = computed(() => store.devices)
const readings = computed(() => envReadings(devices.value))
const classroomName = computed(() => {
  const s = store.spaces.find((x) => x.space_id === store.spaceId)
  return (s && s.name) || '我的教室'
})
const roleName = computed(() => (store.user && store.user.name) || '教师')

const onlineList = computed(() => devices.value.filter((d) => d.status === 'online'))
const offlineList = computed(() => devices.value.filter((d) => d.status !== 'online'))

const shownSessions = computed(() =>
  sessionsExpanded.value ? sessions.value : sessions.value.slice(0, 4)
)

const wsDotClass = computed(() => ({
  online: 'dot--green',
  connecting: 'dot--orange',
  reconnecting: 'dot--orange',
  offline: 'dot--gray',
}[store.wsStatus] || 'dot--gray'))
const wsText = computed(() => ({
  online: '在线',
  connecting: '连接中',
  reconnecting: '重连中',
  offline: '离线',
}[store.wsStatus] || '离线'))

function lastSeenText(d) {
  void store.tick
  return d.last_seen ? fromNow(d.last_seen) : ''
}
function timeText(t) {
  void store.tick
  return fromNow(t)
}

async function refreshAll(showLoading = false) {
  if (!requireAuth()) return
  permOk.value = can('device:control')
  if (!permOk.value) { loading.value = false; return }

  if (!store.spaceId) { loading.value = false; loadError.value = ''; return }
  if (showLoading) { loading.value = true }
  loadError.value = ''

  const devRes = await getDevices(store.spaceId)
  if (devRes.error) {
    loadError.value = devRes.error.message
  } else {
    setDevices(devRes.data || [])
  }

  const sesRes = await getFaceSessions({ space_id: store.spaceId })
  if (!sesRes.error) sessions.value = sesRes.data || []

  loading.value = false
  uni.stopPullDownRefresh()
}

/* 告警角标 */
async function loadBadge() {
  const res = await getAlarms({ status: 'pending', space_id: store.spaceId, page: 1, page_size: 1 })
  if (!res.error && res.data) store.pendingAlarmCount = res.data.total || 0
}

/* onShow 重新拉数据对齐状态（小程序切后台断连，进入页面时的数据不保证有效） */
onShow(() => {
  if (!requireAuth()) return
  if (!can('device:control')) { permOk.value = false; loading.value = false; return }
  connectWs(store.spaceId)
  refreshAll(true)
  loadBadge()
})

onPullDownRefresh(() => refreshAll())

onUnmounted(() => clearInterval(clockTimer))

/* 场景快捷控制（二次确认在 runScene 内） */
function onScene(key) {
  runScene(key, store.devices)
}

/* 订阅消息授权：必须由用户点击行为直接触发，onLoad 不弹 */
function requestSub() {
  // #ifdef MP-WEIXIN
  if (!SUBSCRIBE_TMPL_IDS[0] || SUBSCRIBE_TMPL_IDS[0] === 'TEMPLATE_ID_PLACEHOLDER') {
    uni.showToast({ title: '请先在 api/config.js 配置订阅模板 ID', icon: 'none' })
    return
  }
  uni.requestSubscribeMessage({
    tmplIds: SUBSCRIBE_TMPL_IDS,
    success: (res) => {
      if (res[SUBSCRIBE_TMPL_IDS[0]] === 'accept') {
        setSubscribed(true)
        dismissSubGuide()
        uni.showToast({ title: '已开启推送（每次授权可收到一条通知）', icon: 'none' })
      } else {
        uni.showToast({ title: '已拒绝，可稍后在「我的」中再次开启', icon: 'none' })
      }
    },
    fail: () => uni.showToast({ title: '授权未完成', icon: 'none' }),
  })
  // #endif
  // #ifndef MP-WEIXIN
  uni.showToast({ title: '请在微信小程序中完成订阅授权', icon: 'none' })
  // #endif
}
function dismissSubGuide() {
  showSubGuide.value = false
  uni.setStorageSync('sc_sub_guide_dismissed', 'yes')
}

function goHome() {
  if (isAdmin()) uni.redirectTo({ url: '/pages/admin/overview' })
  else uni.reLaunch({ url: '/pages/login/login' })
}
function goProfile() {
  uni.navigateTo({ url: '/pages/profile/profile' })
}
</script>

<style lang="scss" scoped>
.page { padding-bottom: 40rpx; }

.head {
  margin: 20rpx $gap;
  background: linear-gradient(135deg, #6B93FF, $primary);
  border-radius: $radius;
  padding: 32rpx;
  color: #fff;
  display: flex;
  align-items: center;
  justify-content: space-between;
  box-shadow: 0 8rpx 24rpx rgba(79, 124, 255, 0.25);
}
.head__name-row { display: flex; align-items: center; gap: 14rpx; }
.head__name { font-size: $fs-xl; font-weight: 700; }
.head__time { display: block; font-size: $fs-sm; opacity: 0.85; margin-top: 8rpx; }
.head__ws { display: flex; align-items: center; background: rgba(255, 255, 255, 0.18); border-radius: 40rpx; padding: 10rpx 22rpx; }
.head__ws-text { font-size: $fs-xs; margin-left: 10rpx; }

.subguide {
  display: flex;
  align-items: center;
  margin: 0 $gap;
  background: #FFF9EF;
  border: 2rpx solid rgba(245, 158, 11, 0.35);
  border-radius: $radius-sm;
  padding: 18rpx 22rpx;
  font-size: $fs-sm;
}
.subguide__icon { margin-right: 14rpx; }
.subguide__text { flex: 1; color: $text-2; font-size: $fs-xs; }
.subguide__btn { color: $warning-deep; font-weight: 600; padding: 0 12rpx; }
.subguide__close { color: $text-3; padding: 0 6rpx 0 12rpx; }

.env-empty { text-align: center; color: $text-3; font-size: $fs-sm; padding: 30rpx 0 10rpx; }

.dev-row { display: flex; align-items: center; padding: 18rpx 0; border-bottom: 1rpx solid $border; }
.dev-row:last-child { border-bottom: none; }
.dev-row__name { flex: 1; margin-left: 16rpx; font-size: $fs-md; }
.dev-row__time { font-size: $fs-xs; color: $text-3; }
.dev-row--off { opacity: 0.55; }

.ses-row { display: flex; align-items: center; padding: 18rpx 0; border-bottom: 1rpx solid $border; }
.ses-row:last-child { border-bottom: none; }
.ses-row__avatar {
  width: 64rpx; height: 64rpx; border-radius: 50%;
  background: $primary-light; color: $primary;
  display: flex; align-items: center; justify-content: center;
  font-size: $fs-md; font-weight: 600; margin-right: 18rpx;
}
.ses-row__main { flex: 1; display: flex; align-items: center; gap: 12rpx; }
.ses-row__name { font-size: $fs-md; font-weight: 500; }
.ses-row__side { display: flex; align-items: center; gap: 12rpx; }
.ses-row__time { font-size: $fs-xs; color: $text-3; }

.bottom-bar {
  position: fixed;
  left: $gap;
  right: $gap;
  bottom: calc(130rpx + constant(safe-area-inset-bottom));
  bottom: calc(130rpx + env(safe-area-inset-bottom));
  z-index: 997;
}
</style>
