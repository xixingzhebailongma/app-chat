<template>
  <view class="cv" @tap="onBackdropTap">
    <offline-banner :ws="true" />

    <state-view
      :loading="loading"
      :error="loadError"
      :empty="noClassroom"
      :perm="permOk === false ? false : null"
      perm-text="此页面为教师端首页，当前账号无权访问"
      perm-action="返回"
      empty-text="还未绑定教室"
      empty-desc="请联系管理员在后台为您绑定教室后使用"
      empty-action="联系管理员说明"
      empty-icon="room"
      @retry="refreshAll(true)"
      @perm-action="onPermBack"
      @empty-action="goMine"
    >
      <!-- 顶部浅色 Hero：教室名 + 在线统计 + 同步时间 + 连接状态 -->
      <view class="cv__hero">
        <view class="cv__hero-head">
          <view class="cv__name-row" @tap="goMine">
            <text class="cv__name">{{ classroomName }}</text>
            <view class="cv__role-tag">{{ roleName }}</view>
          </view>
          <view class="cv__ws">
            <view class="cv__ws-dot" :class="['cv__ws-dot--' + wsTone, { 'sc-breathe': store.wsStatus === 'online' }]" />
            <text class="cv__ws-text" :class="'cv__ws-text--' + wsTone">{{ wsText }}</text>
          </view>
        </view>
        <view class="cv__hero-meta">
          <app-icon name="wifi" :size="22" :color="COLORS.primary" />
          <text class="cv__hero-meta-text">{{ onlineList.length }}/{{ devices.length }} 台在线</text>
          <text class="cv__hero-meta-divider">·</text>
          <text class="cv__hero-meta-text">同步于 {{ syncText }}</text>
        </view>
      </view>

      <!-- 订阅引导条（轻量解释价值，用户点击后才调起授权，不自动弹） -->
      <view v-if="showSubGuide" class="cv__subguide">
        <view class="cv__subguide-icon">
          <app-icon name="bell" :size="32" :color="COLORS.warning" />
        </view>
        <text class="cv__subguide-text">开启告警推送，教室异常第一时间通知您</text>
        <text class="cv__subguide-btn" hover-class="press" @tap="requestSub">去开启</text>
        <text class="cv__subguide-close" hover-class="press" @tap="dismissSubGuide">
          <app-icon name="close" :size="26" :color="COLORS.text3" />
        </text>
      </view>

      <!-- 可自定义卡片区：长按卡片进入编辑态，右上角出现减号可删除 -->
      <view
        v-for="w in widgets"
        :key="w"
        class="card cv__card-rel"
        hover-class="press"
        @longpress="enterEdit"
      >
        <view v-if="editMode" class="cv__w-del" hover-class="cv__w-del--press" @tap.stop="removeWidget(w)">
          <view class="cv__w-del-minus" />
        </view>

        <view class="card__title">
          {{ widgetTitle(w) }}
          <text v-if="!editMode && w === 'access'" class="card__more" hover-class="press" @tap="sessionsExpanded = !sessionsExpanded">
            {{ sessionsExpanded ? '收起' : '查看全部' }}
          </text>
        </view>

        <!-- 场景控制 -->
        <view v-if="w === 'scene'"><scene-bar @scene="onScene" /></view>

        <!-- 开关设备 -->
        <template v-else-if="w === 'switch'">
          <device-row
            v-for="r in switchRows"
            :key="r.device_id"
            :device="r"
            controllable
            @toggle="onToggle"
            @offline-tap="onOfflineTap"
          />
          <view v-if="!switchRows.length" class="cv__empty-line">暂无开关设备</view>
        </template>

        <!-- 环境面板 -->
        <template v-else-if="w === 'panel'">
          <view v-for="r in panelRows" :key="r.device_id" class="cv__panel-item">
            <device-row :device="r" controllable @toggle="onToggle" @offline-tap="onOfflineTap" />
            <view v-if="infoOf(r)" class="cv__panel-info">
              <view v-for="(v, k) in infoOf(r)" :key="k" class="cv__panel-cell">
                <text class="cv__panel-k">{{ k }}</text>
                <text class="cv__panel-v num">{{ v }}</text>
              </view>
            </view>
          </view>
          <view v-if="!panelRows.length" class="cv__empty-line">暂无环境面板设备</view>
        </template>

        <!-- 传感器 -->
        <view v-else-if="w === 'sensor'"><env-grid :devices="sensorDevices" /></view>

        <!-- 今日进出 -->
        <template v-else-if="w === 'access'">
          <view v-for="(s, i) in shownSessions" :key="i" class="cv__ses">
            <text class="cv__ses-avatar">{{ (s.name || '?').slice(0, 1) }}</text>
            <view class="cv__ses-main">
              <text class="cv__ses-name">{{ s.name }}</text>
              <view class="tag" :class="s.auth_type === 'face' ? 'tag--blue' : 'tag--gray'">
                {{ s.auth_type === 'face' ? '人脸' : '刷卡' }}
              </view>
            </view>
            <view class="cv__ses-side">
              <text class="cv__ses-time">{{ timeText(s.time) }}</text>
              <view v-if="s.result === 'denied'" class="tag tag--red">拒绝</view>
            </view>
          </view>
          <view v-if="!sessions.length" class="cv__empty-line">今日暂无进出记录</view>
        </template>
      </view>

      <!-- 最底部：添加新卡片入口 -->
      <view class="card cv__add-card" hover-class="cv__add-card--press" @tap.stop="openPicker">
        <view class="cv__add-ring">
          <view class="cv__add-plus" />
        </view>
        <text class="cv__add-text">添加卡片</text>
      </view>

      <!-- 卡片管理入口 -->
      <view class="cv__edit-bar">
        <view v-if="editMode" class="cv__edit-action" hover-class="press" @tap.stop="toggleEdit">完成</view>
        <view v-else class="cv__edit-action cv__edit-action--ghost" hover-class="press" @tap="toggleEdit">自定义卡片</view>
      </view>

      <!-- 添加卡片弹窗 -->
      <view v-if="showPicker" class="cv__mask" @tap="closePicker">
        <view class="cv__sheet" @tap.stop>
          <view class="cv__sheet-title">添加卡片</view>
          <view v-if="!widgetOptions.length" class="cv__empty-line" style="padding: 40rpx 0;">暂无可添加卡片</view>
          <view
            v-for="opt in widgetOptions"
            :key="opt.type"
            class="cv__opt"
            :class="{ 'cv__opt--off': opt.disabled, 'cv__opt--on': opt.added }"
            hover-class="press"
            @tap="onPick(opt)"
          >
            <app-icon :name="opt.icon" :size="40" :color="opt.disabled ? COLORS.text3 : COLORS.primary" />
            <view class="cv__opt-main">
              <text class="cv__opt-name">{{ opt.title }}</text>
              <text class="cv__opt-desc">{{ opt.desc }}</text>
            </view>
            <text class="cv__opt-state">{{ opt.added ? '已添加' : (opt.disabled ? '暂无设备' : '可添加') }}</text>
          </view>
          <view class="cv__sheet-cancel" hover-class="press" @tap="closePicker">取消</view>
        </view>
      </view>
    </state-view>
  </view>
</template>

<script setup>
/**
 * P1 我的教室（教师首页）· 壳层视图（v-show 缓存，切换不重载）
 * 生命周期协议：onEnter（tab 激活/页面 onShow）→ 拉数据对齐；onRefresh（下拉刷新）
 */
import { ref, computed, onMounted, onUnmounted } from 'vue'
import { store, setDevices, setSubscribed } from '../../store'
import { getDevices, getFaceSessions, getAlarms } from '../../api'
import { SUBSCRIBE_TMPL_IDS } from '../../api/config'
import { COLORS } from '../../config/theme'
import { connectWs } from '../../services/ws'
import { can, isAdmin, requireAuth, gotoTab } from '../../utils/permission'
import { fromNow, fmtClockSec, weekdayToday, WEEKDAY_NAMES } from '../../utils/time'
import { runScene } from '../../utils/scene'
import { WIDGETS, WIDGET_MAP, loadWidgets, saveWidgets } from '../../utils/widgets'
import { isSwitchDevice, toDeviceRow } from '../../utils/device'
import { parseInfo } from '../../utils/sensor'
import { execDeviceCommand } from '../../services/cmd'
import { toastError } from '../../api/request'

const loading = ref(true)
const loadError = ref('')
const permOk = ref(null)
const noClassroom = ref(false)
const inited = ref(false)
const sessions = ref([])
const sessionsExpanded = ref(false)
const showSubGuide = ref(
  uni.getStorageSync('sc_sub_guide_dismissed') !== 'yes' && !store.subscribed
)

/* 可自定义卡片：用户配置（持久化） + 编辑态 + 添加弹窗 */
const widgets = ref(loadWidgets())
const editMode = ref(false)
const showPicker = ref(false)

/* 实时时钟 */
const now = ref(Date.now())
let clockTimer = null
const clock = computed(() => fmtClockSec(new Date(now.value)))
const weekdayText = computed(() => WEEKDAY_NAMES[weekdayToday()])
const syncText = computed(() => {
  const list = devices.value.filter((d) => d.last_seen)
  if (!list.length) return '刚刚'
  const latest = list.slice().sort((a, b) => new Date(b.last_seen) - new Date(a.last_seen))[0].last_seen
  return fromNow(latest)
})

const devices = computed(() => store.devices)
const classroomName = computed(() => {
  const s = store.spaces.find((x) => x.space_id === store.spaceId)
  return (s && s.name) || '我的教室'
})
const roleName = computed(() => (store.user && store.user.name) || '教师')

const onlineList = computed(() => devices.value.filter((d) => d.status === 'online'))
const offlineList = computed(() => devices.value.filter((d) => d.status !== 'online'))
const contentReady = computed(() => permOk.value !== false && !loading.value && !loadError.value && !noClassroom.value)

const shownSessions = computed(() =>
  sessionsExpanded.value ? sessions.value : sessions.value.slice(0, 4)
)

/* 设备控制模块的同款分组，供卡片复用 */
const switchRows = computed(() =>
  store.devices.filter((d) => d.device_type === 'zigbee' && isSwitchDevice(d)).map(toDeviceRow)
)
const panelRows = computed(() =>
  store.devices.filter((d) => String(d.device_type).startsWith('fuhe')).map(toDeviceRow)
)
const sensorDevices = computed(() =>
  store.devices.filter(
    (d) => d.zb_type === 'sensor' || (!isSwitchDevice(d) && !String(d.device_type).startsWith('fuhe'))
  )
)

/* 卡片元信息 / 可用判断 / 弹窗选项 */
function widgetTitle(t) { return (WIDGET_MAP[t] || {}).title || t }
function widgetAvailable(t) {
  if (t === 'scene' || t === 'access') return true
  if (t === 'switch') return switchRows.value.length > 0
  if (t === 'panel') return panelRows.value.length > 0
  if (t === 'sensor') return sensorDevices.value.length > 0
  return false
}
const widgetOptions = computed(() => WIDGETS.map((w) => ({
  ...w,
  added: widgets.value.includes(w.type),
  disabled: widgets.value.includes(w.type) || !widgetAvailable(w.type),
})))
function infoOf(r) { return parseInfo(r._raw) }

function addWidget(t) {
  if (!widgets.value.includes(t)) { widgets.value.push(t); saveWidgets(widgets.value) }
}
function removeWidget(t) {
  widgets.value = widgets.value.filter((x) => x !== t)
  saveWidgets(widgets.value)
}
function toggleEdit() {
  editMode.value = !editMode.value
  if (!editMode.value) showPicker.value = false
}
function enterEdit() { editMode.value = true }
function onBackdropTap() { if (editMode.value) { editMode.value = false; showPicker.value = false } }
function openPicker() { showPicker.value = true }
function closePicker() { showPicker.value = false }
function onPick(opt) { if (opt.disabled) return; addWidget(opt.type) }

const WS_TEXT = { online: '在线', connecting: '连接中', reconnecting: '重连中', offline: '离线' }
const WS_TONE = { online: 'on', connecting: 'wait', reconnecting: 'wait', offline: 'off' }
const wsText = computed(() => WS_TEXT[store.wsStatus] || '离线')
const wsTone = computed(() => WS_TONE[store.wsStatus] || 'off')

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

  if (!store.spaceId) {
    noClassroom.value = true
    loading.value = false
    loadError.value = ''
    return
  }
  noClassroom.value = false
  if (showLoading) loading.value = true
  loadError.value = ''

  /* 设备列表 / 进出记录 / 告警角标三者并行，取最慢一路即可完成首屏 */
  const [devRes, sesRes] = await Promise.all([
    getDevices(store.spaceId),
    getFaceSessions({ space_id: store.spaceId }),
  ])
  if (devRes.error) {
    loadError.value = devRes.error.message
  } else {
    setDevices(devRes.data || [])
  }
  if (!sesRes.error) sessions.value = sesRes.data || []

  loading.value = false
  inited.value = true
  uni.stopPullDownRefresh()
}

/* 告警角标（壳层 TabBar 徽标） */
async function loadBadge() {
  const res = await getAlarms({ status: 'pending', space_id: store.spaceId, page: 1, page_size: 1 })
  if (!res.error && res.data) store.pendingAlarmCount = res.data.total || 0
}

/* 壳层协议 */
function onEnter() {
  if (!requireAuth()) return
  if (!can('device:control')) { permOk.value = false; loading.value = false; return }
  connectWs(store.spaceId)
  refreshAll(!inited.value)
  loadBadge()
}
function onRefresh() { refreshAll() }

onMounted(() => {
  clockTimer = setInterval(() => { now.value = Date.now() }, 1000)
})
onUnmounted(() => clearInterval(clockTimer))

/* 场景快捷控制（二次确认在 runScene 内） */
function onScene(key) {
  runScene(key, store.devices)
}

/* 开关控制（卡片内设备行用，三段式确认） */
async function onToggle({ device }) {
  const raw = device._raw || device
  const command = raw.app === 'on' ? 'off' : 'on'
  const { error } = await execDeviceCommand(raw, command)
  if (error) toastError(error)
}
function onOfflineTap() {
  uni.showToast({ title: '设备离线，暂时无法控制', icon: 'none' })
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

function onPermBack() {
  if (isAdmin()) gotoTab('overview')
  else uni.reLaunch({ url: '/pages/login/login' })
}
function goMine() {
  gotoTab('mine')
}

defineExpose({ onEnter, onRefresh })
</script>

<style lang="scss" scoped>
/* 页面模块流式排列，底部场景栏不再 fixed，正常留白即可 */
.cv { padding-bottom: $sp-24; }

/* 浅色 Hero 头卡：品牌浅蓝渐变底，清爽不死黑 */
.cv__hero {
  margin: $sp-20 $page-pad $sp-24;
  background: linear-gradient(135deg, #F8FAFF 0%, #EBF3FF 100%);
  border-radius: $r-card;
  padding: $card-pad;
  color: $text-1;
  box-shadow: 0 8rpx 28rpx rgba(0, 122, 255, 0.08);
  border: 1rpx solid rgba(0, 122, 255, 0.12);
}
.cv__hero-head {
  @include flex(space-between, center);
  margin-bottom: $sp-20;
}
.cv__name-row {
  @include flex(flex-start, center);
  gap: 14rpx;
  min-width: 0;
}
.cv__name { font-size: $fs-page-title; font-weight: 700; color: $text-1; @include ellipsis(1); }
.cv__role-tag {
  font-size: $fs-tag;
  line-height: 34rpx;
  padding: 0 14rpx;
  border-radius: $r-tag;
  background: $primary-light;
  color: $primary;
  flex: none;
}
.cv__hero-meta {
  @include flex(flex-start, center);
  gap: $sp-8;
  color: $text-2;
  font-size: $fs-aux;
}
.cv__hero-meta-text { color: $text-2; }
.cv__hero-meta-divider { color: $text-3; margin: 0 4rpx; }
.cv__ws {
  @include flex(flex-start, center);
  background: #fff;
  border-radius: 40rpx;
  padding: 10rpx 22rpx;
  flex: none;
  box-shadow: 0 2rpx 8rpx rgba(0, 0, 0, 0.04);
}
.cv__ws-dot {
  width: 14rpx;
  height: 14rpx;
  border-radius: 50%;
}
.cv__ws-dot--on { background: $success; }
.cv__ws-dot--wait { background: $warning; }
.cv__ws-dot--off { background: $disabled; }
.cv__ws-text { font-size: $fs-tag; margin-left: 10rpx; color: $text-2; }
.cv__ws-text--on { color: $success; }
.cv__ws-text--wait { color: $warning; }
.cv__ws-text--off { color: $disabled; }

/* 分区标题 */
.cv__section-head {
  @include flex(space-between, center);
  margin: $sp-24 $page-pad 0;
  font-size: $fs-card-title;
  font-weight: 600;
  color: $text-1;
}
.cv__section-more { font-size: $fs-aux; font-weight: 400; color: $text-3; }

.cv__subguide {
  @include flex(flex-start, center);
  margin: $sp-20 $page-pad 0;
  background: $warning-light;
  border-radius: $r-card;
  padding: 18rpx 22rpx;
}
.cv__subguide-icon { margin-right: $sp-12; }
.cv__subguide-text { flex: 1; color: $text-2; font-size: $fs-tag; }
.cv__subguide-btn { color: $warning; font-weight: 600; padding: 0 12rpx; }
.cv__subguide-close { padding: 0 6rpx 0 12rpx; }

.cv__dev {
  @include flex(flex-start, center);
  padding: 18rpx 0;
  border-bottom: 1rpx solid $line;
}
.cv__dev:last-child { border-bottom: none; }
.cv__dev-dot {
  width: 14rpx;
  height: 14rpx;
  border-radius: 50%;
  flex: none;
}
.cv__dev-dot--on { background: $success; }
.cv__dev-dot--off { background: $disabled; }
.cv__dev-name { flex: 1; margin-left: $sp-16; font-size: $fs-body; color: $text-1; }
.cv__dev-time { font-size: $fs-tag; color: $text-3; }
.cv__dev--off { opacity: 0.55; }

.cv__ses {
  @include flex(flex-start, center);
  padding: 18rpx 0;
  border-bottom: 1rpx solid $line;
}
.cv__ses:last-child { border-bottom: none; }
.cv__ses-avatar {
  width: 64rpx;
  height: 64rpx;
  border-radius: 50%;
  background: $primary-light;
  color: $primary;
  @include flex(center, center);
  font-size: $fs-body;
  font-weight: 600;
  margin-right: 18rpx;
}
.cv__ses-main { flex: 1; @include flex(flex-start, center); gap: 12rpx; min-width: 0; }
.cv__ses-name { font-size: $fs-body; font-weight: 500; }
.cv__ses-side { @include flex(flex-start, center); gap: 12rpx; }
.cv__ses-time { font-size: $fs-tag; color: $text-3; }

.cv__empty-line {
  text-align: center;
  color: $text-3;
  font-size: $fs-aux;
  padding: 30rpx 0 10rpx;
}

/* 卡片相对定位：给右上角减号提供参考 */
.cv__card-rel { position: relative; }

/* 编辑态：卡片右上角红色减号 */
.cv__w-del {
  position: absolute;
  top: -10rpx;
  right: -10rpx;
  width: 40rpx;
  height: 40rpx;
  border-radius: 50%;
  background: $danger;
  z-index: 10;
  @include flex(center, center);
  box-shadow: 0 2rpx 10rpx rgba(255, 45, 85, 0.35);
}
.cv__w-del--press { transform: scale(0.92); opacity: 0.9; }
.cv__w-del-minus {
  width: 18rpx;
  height: 4rpx;
  border-radius: 2rpx;
  background: #fff;
}

/* 最底部：添加卡片入口 */
.cv__add-card {
  @include flex(center, center);
  flex-direction: column;
  gap: 14rpx;
  padding: 38rpx 0;
  border: 2rpx dashed $line !important;
  background: $bg-page !important;
}
.cv__add-card--press { opacity: 0.75; }
.cv__add-ring {
  width: 72rpx;
  height: 72rpx;
  border-radius: 50%;
  background: $primary-light;
  @include flex(center, center);
}
.cv__add-plus {
  width: 28rpx;
  height: 4rpx;
  border-radius: 2rpx;
  background: $primary;
  position: relative;
}
.cv__add-plus::after {
  content: '';
  position: absolute;
  top: -12rpx;
  left: 50%;
  transform: translateX(-50%);
  width: 4rpx;
  height: 28rpx;
  border-radius: 2rpx;
  background: $primary;
}
.cv__add-text { font-size: $fs-aux; color: $text-3; }

/* 卡片管理入口栏 */
.cv__edit-bar {
  @include flex(center, center);
  gap: $sp-16;
  margin: $sp-24 $page-pad $sp-8;
}
.cv__edit-action {
  @include flex(center, center);
  flex: 1;
  height: 80rpx;
  border-radius: $r-tag;
  background: $primary-light;
  color: $primary;
  font-size: $fs-body;
  font-weight: 600;
  border: 1rpx solid rgba(0, 122, 255, 0.2);
}
.cv__edit-action--primary { background: $primary; color: #fff; border-color: $primary; }
.cv__edit-action--ghost { flex: none; padding: 0 $sp-32; background: transparent; color: $primary; border: 1rpx solid $primary; }

/* 添加卡片弹窗（底部 sheet + 遮罩） */
.cv__mask {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: rgba(0, 0, 0, 0.4);
  z-index: 900;
  @include flex(center, flex-end);
}
.cv__sheet {
  background: $bg-card;
  border-radius: $r-card $r-card 0 0;
  padding: $card-pad $card-pad calc($card-pad + constant(safe-area-inset-bottom));
  padding: $card-pad $card-pad calc($card-pad + env(safe-area-inset-bottom));
  width: 100%;
  max-height: 78vh;
  overflow-y: auto;
}
.cv__sheet-title {
  font-size: $fs-card-title;
  font-weight: 700;
  color: $text-1;
  margin-bottom: $sp-16;
}
.cv__opt {
  @include flex(flex-start, center);
  gap: $sp-20;
  padding: $sp-24 0;
  border-bottom: 1rpx solid $line;
}
.cv__opt--off { opacity: 0.5; }
.cv__opt-main { flex: 1; @include flex(flex-start, flex-start, column); min-width: 0; }
.cv__opt-name { font-size: $fs-body; font-weight: 600; color: $text-1; }
.cv__opt-desc { font-size: $fs-aux; color: $text-3; margin-top: 4rpx; }
.cv__opt-state { font-size: $fs-tag; color: $text-3; }
.cv__opt--on .cv__opt-state { color: $primary; }
.cv__sheet-cancel {
  text-align: center;
  margin-top: $sp-20;
  padding: $sp-20 0;
  color: $text-2;
  font-size: $fs-body;
  background: $bg-page;
  border-radius: $r-tag;
}

/* 卡片内环境面板信息行（复用设备控制页结构） */
.cv__panel-item { margin-bottom: $sp-12; }
.cv__panel-info {
  background: $bg-page;
  border-radius: $r-icon;
  padding: 18rpx 24rpx;
  margin-top: 12rpx;
  @include flex(flex-start, center);
  flex-wrap: wrap;
  gap: 12rpx 40rpx;
}
.cv__panel-cell { @include flex(flex-start, baseline); }
.cv__panel-k {
  font-size: $fs-tag;
  color: $text-3;
  margin-right: 10rpx;
  text-transform: uppercase;
}
.cv__panel-v { font-size: $fs-aux; font-weight: 600; color: $text-1; }
</style>
