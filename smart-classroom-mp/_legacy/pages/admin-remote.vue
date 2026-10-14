<template>
  <view class="page">
    <OfflineBanner :ws="true" />

    <StateView
      :loading="loading"
      :error="loadError"
      :perm="permOk === false ? false : null"
      perm-text="远程控制为管理员功能"
      empty-text="请先选择教室"
      empty-desc="从总览或告警详情进入可自动选中教室"
      empty-action="返回总览"
      empty-icon="🎛️"
      @retry="refresh(true)"
      @perm-action="goHome"
      @empty-action="goHome"
    >
      <!-- 第一步：选教室 -->
      <view class="card">
        <view class="card__title">① 选择教室</view>
        <view class="space-chips">
          <view
            v-for="s in store.spaces"
            :key="s.space_id"
            class="chip"
            :class="{ 'chip--on': s.space_id === spaceId }"
            @tap="switchSpace(s.space_id)"
          >{{ s.name }}</view>
        </view>
      </view>

      <!-- 第二步：选设备控制 -->
      <view class="card">
        <view class="card__title">② 设备控制<text class="card__more">指令经云端鉴权后转发边侧</text></view>
        <view v-for="d in switchDevices" :key="d.device_id">
          <DeviceCard :device="d" toggleable @toggle="onToggle" />
        </view>
        <view v-if="!switchDevices.length" class="empty-line">该教室暂无可控设备</view>
      </view>

      <!-- 高风险操作区：位置 + 颜色 + 文案三重区分，防误触 -->
      <view class="danger-zone">
        <view class="danger-zone__title">⚠️ 高风险操作</view>
        <view class="danger-zone__sub">以下操作影响教室安全与授课连续性，请谨慎执行</view>
        <button class="c-btn c-btn--danger dz-btn" @tap="forceOpenDoor">🔓 强制开门</button>
        <button class="c-btn c-btn--danger dz-btn" @tap="rebootScreen">🔄 远程重启综合屏</button>
      </view>
    </StateView>

    <view class="safe-bottom"></view>
    <TabBar current="/pages/admin/remote" />
  </view>
</template>

<script setup>
/**
 * P7 远程控制（管理员）：先选教室 → 再选设备，两级选择
 * 高风险操作（强制开门/远程重启）：二次确认 + 警示色 + 分区 + 底部位置，三重区分防误触
 */
import { ref, computed } from 'vue'
import { onLoad, onShow, onPullDownRefresh } from '@dcloudio/uni-app'
import { getDevices, rebootDevice, getSipEndpoints } from '../../api'
import { store, setDevices } from '../../store'
import { connectWs } from '../../services/ws'
import { execDeviceCommand } from '../../services/cmd'
import { toastError } from '../../api/request'
import { can, requireAuth } from '../../utils/permission'
import { isSwitchDevice } from '../../utils/device'
import { confirmModal } from '../../utils/scene'
import DeviceCard from '../../components/DeviceCard.vue'
import OfflineBanner from '../../components/OfflineBanner.vue'
import StateView from '../../components/StateView.vue'
import TabBar from '../../components/TabBar.vue'

const spaceId = ref('')
const loading = ref(true)
const loadError = ref('')
const permOk = ref(null)

const switchDevices = computed(() =>
  store.devices.filter((d) => isSwitchDevice(d))
)

async function refresh(showLoading = false) {
  if (!requireAuth()) return
  permOk.value = can('device:remote')
  if (!permOk.value) { loading.value = false; return }
  if (!store.spaces.length) {
    // 教室目录兜底：SIP 端点接口（免认证）提供 space_id 列表
    const res = await getSipEndpoints()
    if (!res.error && res.data) {
      store.spaces = (res.data.classrooms || []).map((c) => ({ space_id: c.space_id, name: c.name }))
    }
  }
  if (!spaceId.value && store.spaces.length) spaceId.value = store.spaces[0].space_id
  if (!spaceId.value) { loading.value = false; return }
  if (showLoading) loading.value = true
  loadError.value = ''
  const res = await getDevices(spaceId.value)
  if (res.error) loadError.value = res.error.message
  else setDevices(res.data || [])
  loading.value = false
  uni.stopPullDownRefresh()
}

onLoad((options) => {
  if (options && options.space_id) spaceId.value = options.space_id
})
onShow(() => {
  if (!requireAuth()) return
  if (spaceId.value) connectWs(spaceId.value)
  refresh(true)
})
onPullDownRefresh(() => refresh())

function switchSpace(id) {
  if (spaceId.value === id) return
  spaceId.value = id
  store.devices = []
  connectWs(id)
  refresh(true)
}

async function onToggle(device) {
  const command = device.app === 'on' ? 'off' : 'on'
  const { error } = await execDeviceCommand(device, command)
  if (error) toastError(error)
}

/* 高风险：强制开门（双重确认） */
async function forceOpenDoor() {
  const door = store.devices.find((d) => /门/.test((d.label || '') + d.device_id))
  if (!door) {
    uni.showToast({ title: '该教室未接入可控门锁', icon: 'none' })
    return
  }
  if (door.status !== 'online') {
    uni.showToast({ title: '门锁设备离线，无法执行', icon: 'none' })
    return
  }
  const first = await confirmModal('强制开门', `即将强制开启「${door.label || door.device_id}」，门锁将保持开启状态，请确认现场安全。`, '继续')
  if (!first.confirm) return
  const second = await confirmModal('再次确认', '强制开门为高风险操作，确认执行？', '确认开门')
  if (!second.confirm) return
  const { error } = await execDeviceCommand(door, 'on')
  if (error) toastError(error)
  else uni.showToast({ title: '开门指令已下发', icon: 'none' })
}

/* 高风险：远程重启综合屏（双重确认） */
async function rebootScreen() {
  const screen = store.devices.find((d) => d.device_type === 'fuhe-screen')
  if (!screen) {
    uni.showToast({ title: '该教室未接入综合屏', icon: 'none' })
    return
  }
  const first = await confirmModal('远程重启', `即将远程重启「${screen.label || '综合屏'}」，重启期间屏幕中断约 1 分钟。`, '继续')
  if (!first.confirm) return
  const second = await confirmModal('再次确认', '远程重启将中断当前授课画面，确认执行？', '确认重启')
  if (!second.confirm) return
  uni.showLoading({ title: '重启指令下发中' })
  const { error } = await rebootDevice(screen.device_id)
  uni.hideLoading()
  if (error) return toastError(error)
  uni.showToast({ title: '重启指令已下发', icon: 'none' })
}

function goHome() {
  uni.redirectTo({ url: '/pages/admin/overview' })
}
</script>

<style lang="scss" scoped>
.page { padding-bottom: 40rpx; }

.space-chips { display: flex; flex-wrap: wrap; gap: 16rpx; }
.chip {
  padding: 12rpx 30rpx;
  border-radius: 40rpx;
  background: #fff;
  border: 2rpx solid $border;
  font-size: $fs-sm;
  color: $text-2;
}
.chip--on { background: $primary; border-color: $primary; color: #fff; font-weight: 600; }

.empty-line { text-align: center; color: $text-3; font-size: $fs-sm; padding: 26rpx 0 8rpx; }

/* 高风险操作区：位置靠底 + 红色警示 + 明确文案（三重区分） */
.danger-zone {
  margin: 40rpx $gap 20rpx;
  border: 3rpx dashed rgba(239, 68, 68, 0.45);
  border-radius: $radius;
  padding: 30rpx;
  background: #FFF9F9;
}
.danger-zone__title { color: $danger-deep; font-size: $fs-lg; font-weight: 700; }
.danger-zone__sub { color: $text-3; font-size: $fs-xs; margin: 10rpx 0 26rpx; line-height: 36rpx; }
.dz-btn { display: block; margin: 0 0 20rpx; }
.dz-btn:last-child { margin-bottom: 0; }
</style>
