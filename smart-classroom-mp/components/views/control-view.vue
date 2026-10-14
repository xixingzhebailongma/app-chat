<template>
  <view class="ct">
    <offline-banner :ws="true" />

    <state-view
      :loading="loading"
      :error="loadError"
      :empty="!loading && !loadError && !allDevices.length"
      :perm="permOk === false ? false : null"
      perm-text="此页面为教师端功能，当前账号无权访问"
      empty-text="教室内暂无设备"
      empty-desc="设备将在接入网关后自动出现"
      empty-action="返回我的教室"
      empty-icon="codec"
      @retry="refresh(true)"
      @perm-action="goHome"
      @empty-action="goHome"
    >
      <!-- 顶部固定「场景控制」区：视觉上比设备列表更突出 -->
      <view class="ct__scene-zone">
        <text class="ct__scene-title">场景控制</text>
        <scene-bar @scene="onScene" />
      </view>

      <!-- 开关类设备 -->
      <view class="card" v-if="switchRows.length">
        <view class="card__title">开关设备<text class="card__more">{{ switchRows.length }} 台</text></view>
        <device-row
          v-for="r in switchRows"
          :key="r.device_id"
          :device="r"
          controllable
          @toggle="onToggle"
          @offline-tap="onOfflineTap"
        />
      </view>

      <!-- 环境面板（fuhe 类：app 开关 + info 运行指标） -->
      <view class="card" v-if="panelRows.length">
        <view class="card__title">环境面板<text class="card__more">{{ panelRows.length }} 台</text></view>
        <view v-for="r in panelRows" :key="r.device_id" class="ct__panel-item">
          <device-row :device="r" controllable @toggle="onToggle" @offline-tap="onOfflineTap" />
          <view v-if="infoOf(r)" class="ct__panel-info">
            <view v-for="(v, k) in infoOf(r)" :key="k" class="ct__panel-cell">
              <text class="ct__panel-k">{{ k }}</text>
              <text class="ct__panel-v num">{{ v }}</text>
            </view>
          </view>
        </view>
      </view>

      <!-- 传感器类（只读） -->
      <view class="card" v-if="sensorDevices.length">
        <view class="card__title">传感器<text class="card__more">只读 · 实时上报</text></view>
        <env-grid :devices="sensorDevices" />
      </view>
    </state-view>

    <view class="safe-bottom" />
  </view>
</template>

<script setup>
/**
 * P2 设备控制页（教师）· 壳层视图
 * - 分组：开关类 / 环境面板 / 传感器类
 * - 离线设备置灰不可点；控制下发走三段式确认（pending 有视觉反馈，不做乐观更新）
 * - 场景按钮二次确认 + 文案说明影响范围（runScene 内实现）
 */
import { ref, computed, onMounted, onUnmounted } from 'vue'
import { store, setDevices } from '../../store'
import { getDevices } from '../../api'
import { connectWs } from '../../services/ws'
import { execDeviceCommand } from '../../services/cmd'
import { toastError } from '../../api/request'
import { can, requireAuth, gotoTab } from '../../utils/permission'
import { isSwitchDevice, toDeviceRow } from '../../utils/device'
import { parseInfo } from '../../utils/sensor'
import { runScene } from '../../utils/scene'

const loading = ref(true)
const loadError = ref('')
const permOk = ref(null)
const inited = ref(false)

const allDevices = computed(() => store.devices)
const switchRows = computed(() =>
  allDevices.value
    .filter((d) => d.device_type === 'zigbee' && isSwitchDevice(d))
    .map(toDeviceRow)
)
const panelRows = computed(() =>
  allDevices.value
    .filter((d) => String(d.device_type).startsWith('fuhe'))
    .map(toDeviceRow)
)
const sensorDevices = computed(() =>
  allDevices.value.filter(
    (d) => d.zb_type === 'sensor' || (!isSwitchDevice(d) && !String(d.device_type).startsWith('fuhe'))
  )
)

function infoOf(r) { return parseInfo(r._raw) }

async function refresh(showLoading = false) {
  if (!requireAuth()) return
  permOk.value = can('device:control')
  if (!permOk.value) { loading.value = false; return }
  if (showLoading) loading.value = true
  loadError.value = ''
  const res = await getDevices(store.spaceId)
  if (res.error) loadError.value = res.error.message
  else setDevices(res.data || [])
  loading.value = false
  inited.value = true
  uni.stopPullDownRefresh()
}

/* 壳层协议 */
function onEnter() {
  if (!requireAuth()) return
  if (!can('device:control')) { permOk.value = false; loading.value = false; return }
  connectWs(store.spaceId)
  refresh(!inited.value)
}
function onRefresh() { refresh() }

/* 三段式超时 / 重连对账 → 重新拉快照（壳层常驻，仅注册一次） */
function onRemoteRefresh() { refresh() }
onMounted(() => {
  uni.$on('devices:refresh', onRemoteRefresh)
  uni.$on('ws:resync', onRemoteRefresh)
})
onUnmounted(() => {
  uni.$off('devices:refresh', onRemoteRefresh)
  uni.$off('ws:resync', onRemoteRefresh)
})

/* 开关控制：三段式确认（pending 由 store 驱动，不做乐观更新） */
async function onToggle({ device }) {
  const raw = device._raw || device
  const command = raw.app === 'on' ? 'off' : 'on'
  const { error } = await execDeviceCommand(raw, command)
  if (error) toastError(error)
}
function onOfflineTap() {
  uni.showToast({ title: '设备离线，暂时无法控制', icon: 'none' })
}

function onScene(key) {
  runScene(key, store.devices)
}

function goHome() {
  gotoTab('classroom')
}

defineExpose({ onEnter, onRefresh })
</script>

<style lang="scss" scoped>
.ct { padding-bottom: $sp-16; }

.ct__scene-zone { margin: $sp-20 $page-pad; }
.ct__scene-title {
  display: block;
  font-size: $fs-card-title;
  font-weight: 700;
  color: $text-1;
  margin-bottom: $sp-16;
}
.ct__panel-item { margin-bottom: $sp-12; }
.ct__panel-info {
  background: $bg-page;
  border-radius: $r-icon;
  padding: 18rpx 24rpx;
  margin-top: 12rpx;
  @include flex(flex-start, center);
  flex-wrap: wrap;
  gap: 12rpx 40rpx;
}
.ct__panel-cell { @include flex(flex-start, baseline); }
.ct__panel-k {
  font-size: $fs-tag;
  color: $text-3;
  margin-right: 10rpx;
  text-transform: uppercase;
}
.ct__panel-v { font-size: $fs-aux; font-weight: 600; color: $text-1; }
</style>
