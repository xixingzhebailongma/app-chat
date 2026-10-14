<template>
  <view class="page">
    <OfflineBanner :ws="true" />

    <StateView
      :loading="loading"
      :error="loadError"
      :empty="!loading && !loadError && !allDevices.length"
      :perm="permOk === false ? false : null"
      perm-text="此页面为教师端功能，当前账号无权访问"
      empty-text="教室内暂无设备"
      empty-desc="设备将在接入网关后自动出现"
      empty-action="返回我的教室"
      empty-icon="🔌"
      @retry="refresh(true)"
      @perm-action="goHome"
      @empty-action="goHome"
    >
      <!-- 顶部固定「场景控制」区：视觉上比设备列表更突出 -->
      <view class="scene-zone">
        <text class="scene-zone__title">场景控制</text>
        <SceneBar @scene="onScene" />
      </view>

      <!-- 开关类设备 -->
      <view class="card" v-if="switchDevices.length">
        <view class="card__title">开关设备<text class="card__more">{{ switchDevices.length }} 台</text></view>
        <DeviceCard
          v-for="d in switchDevices"
          :key="d.device_id"
          :device="d"
          toggleable
          @toggle="onToggle"
        />
      </view>

      <!-- 环境面板（fuhe 类：app 开关 + info 运行指标） -->
      <view class="card" v-if="panelDevices.length">
        <view class="card__title">环境面板<text class="card__more">{{ panelDevices.length }} 台</text></view>
        <view v-for="d in panelDevices" :key="d.device_id" class="panel-item">
          <DeviceCard :device="d" toggleable @toggle="onToggle" />
          <view v-if="infoOf(d)" class="panel-info">
            <view v-for="(v, k) in infoOf(d)" :key="k" class="panel-info__cell">
              <text class="panel-info__k">{{ k }}</text>
              <text class="panel-info__v">{{ v }}</text>
            </view>
          </view>
        </view>
      </view>

      <!-- 传感器类（只读） -->
      <view class="card" v-if="sensorDevices.length">
        <view class="card__title">传感器<text class="card__more">只读 · 实时上报</text></view>
        <SensorPanel :devices="sensorDevices" />
      </view>
    </StateView>

    <view class="safe-bottom"></view>
    <TabBar current="/pages/teacher/control" />
  </view>
</template>

<script setup>
/**
 * P2 设备控制页（教师）：
 * - 分组：开关类 / 环境面板 / 传感器类
 * - 离线设备置灰不可点
 * - 控制下发走三段式确认（pending 有视觉反馈）
 * - 场景按钮二次确认 + 文案说明影响范围
 */
import { ref, computed } from 'vue'
import { onShow, onPullDownRefresh, onUnload } from '@dcloudio/uni-app'
import { store, setDevices } from '../../store'
import { getDevices } from '../../api'
import { connectWs } from '../../services/ws'
import { execDeviceCommand } from '../../services/cmd'
import { toastError } from '../../api/request'
import { can, requireAuth } from '../../utils/permission'
import { isSwitchDevice } from '../../utils/device'
import { parseInfo } from '../../utils/sensor'
import { runScene } from '../../utils/scene'
import DeviceCard from '../../components/DeviceCard.vue'
import SensorPanel from '../../components/SensorPanel.vue'
import OfflineBanner from '../../components/OfflineBanner.vue'
import StateView from '../../components/StateView.vue'
import SceneBar from '../../components/SceneBar.vue'
import TabBar from '../../components/TabBar.vue'

const loading = ref(true)
const loadError = ref('')
const permOk = ref(null)

const allDevices = computed(() => store.devices)
const switchDevices = computed(() =>
  allDevices.value.filter((d) => d.device_type === 'zigbee' && isSwitchDevice(d))
)
const panelDevices = computed(() =>
  allDevices.value.filter((d) => String(d.device_type).startsWith('fuhe'))
)
const sensorDevices = computed(() =>
  allDevices.value.filter((d) => d.zb_type === 'sensor' || (!isSwitchDevice(d) && !String(d.device_type).startsWith('fuhe')))
)

function infoOf(d) { return parseInfo(d) }

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
  uni.stopPullDownRefresh()
}

onShow(() => {
  if (!requireAuth()) return
  if (!can('device:control')) { permOk.value = false; loading.value = false; return }
  connectWs(store.spaceId)
  refresh(true)
  // 三段式超时 / 重连对账 → 重新拉快照
  uni.$off('devices:refresh', onRemoteRefresh)
  uni.$on('devices:refresh', onRemoteRefresh)
  uni.$off('ws:resync', onRemoteRefresh)
  uni.$on('ws:resync', onRemoteRefresh)
})

function onRemoteRefresh() { refresh() }

onPullDownRefresh(() => refresh())
onUnload(() => {
  uni.$off('devices:refresh', onRemoteRefresh)
  uni.$off('ws:resync', onRemoteRefresh)
})

/* 开关控制：三段式确认（不做乐观更新，pending 由 store 驱动） */
async function onToggle(device) {
  const command = device.app === 'on' ? 'off' : 'on'
  const { error } = await execDeviceCommand(device, command)
  if (error) toastError(error)
}

function onScene(key) {
  runScene(key, store.devices)
}

function goHome() {
  uni.redirectTo({ url: '/pages/teacher/classroom' })
}
</script>

<style lang="scss" scoped>
.page { padding-bottom: 40rpx; }

.scene-zone {
  margin: 20rpx $gap;
}
.scene-zone__title {
  display: block;
  font-size: $fs-lg;
  font-weight: 700;
  margin-bottom: 18rpx;
}
.panel-item { margin-bottom: 16rpx; }
.panel-info {
  background: #F6F8FB;
  border-radius: $radius-sm;
  padding: 18rpx 24rpx;
  margin-top: 12rpx;
  display: flex;
  flex-wrap: wrap;
  gap: 12rpx 40rpx;
}
.panel-info__cell { display: flex; align-items: baseline; }
.panel-info__k { font-size: $fs-xs; color: $text-3; margin-right: 10rpx; text-transform: uppercase; }
.panel-info__v { font-size: $fs-sm; font-weight: 600; }
</style>
