﻿<template>
  <view class="rv">
    <offline-banner :ws="true" />

    <state-view
      :loading="loading"
      :error="loadError"
      :perm="permOk === false ? false : null"
      perm-text="远程控制为管理员功能"
      empty-text="请先选择教室"
      empty-desc="从总览或告警详情进入可自动选中教室"
      empty-action="返回总览"
      empty-icon="control"
      @retry="refresh(true)"
      @perm-action="goHome"
      @empty-action="goHome"
    >
      <!-- 第一步：选教室 -->
      <view class="card">
        <view class="card__title">① 选择教室</view>
        <view class="rv__chips">
          <view
            v-for="s in store.spaces"
            :key="s.space_id"
            class="rv__chip"
            :class="{ 'rv__chip--on': s.space_id === spaceId }"
            hover-class="press"
            @tap="switchSpace(s.space_id)"
          >{{ s.name }}</view>
        </view>
      </view>

      <!-- 第二步：选设备控制 -->
      <view class="card">
        <view class="card__title">② 设备控制<text class="card__more">指令经云端鉴权后转发边侧</text></view>
        <device-row
          v-for="r in switchRows"
          :key="r.device_id"
          :device="r"
          controllable
          @toggle="onToggle"
          @offline-tap="onOfflineTap"
        />
        <view v-if="!switchRows.length" class="rv__empty-line">该教室暂无可控设备</view>
      </view>

      <!-- 高风险操作区：位置 + 颜色 + 文案三重区分，防误触 -->
      <view class="rv__danger">
        <view class="rv__danger-title">
          <app-icon name="warning" :size="34" :color="COLORS.danger" />
          <text>高风险操作</text>
        </view>
        <view class="rv__danger-sub">以下操作影响教室安全与授课连续性，请谨慎执行</view>
        <button class="c-btn c-btn--danger rv__danger-btn" hover-class="press" @tap="forceOpenDoor">
          <app-icon name="lock" :size="30" :color="COLORS.white" />
          <text>强制开门</text>
        </button>
        <button class="c-btn c-btn--danger rv__danger-btn" hover-class="press" @tap="rebootScreen">
          <app-icon name="restart" :size="30" :color="COLORS.white" />
          <text>远程重启综合屏</text>
        </button>
      </view>
    </state-view>

    <view class="safe-bottom" />
  </view>
</template>

<script setup>
/**
 * P7 远程控制（管理员）· 壳层视图：先选教室 → 再选设备，两级选择
 * 高风险操作（强制开门/远程重启）：二次确认 + 警示色 + 分区 + 底部位置，三重区分防误触
 * 从告警详情/教室详情进入时由壳层传入 space_id（onEnter 的 extra）
 */
import { ref, computed } from 'vue'
import { getDevices, rebootDevice, getSipEndpoints } from '../../api'
import { store, setDevices } from '../../store'
import { connectWs } from '../../services/ws'
import { execDeviceCommand } from '../../services/cmd'
import { toastError } from '../../api/request'
import { can, requireAuth, gotoTab } from '../../utils/permission'
import { isSwitchDevice, toDeviceRow } from '../../utils/device'
import { confirmModal } from '../../utils/scene'
import { COLORS } from '../../config/theme'

const spaceId = ref('')
const loading = ref(true)
const loadError = ref('')
const permOk = ref(null)
const inited = ref(false)

const switchRows = computed(() => store.devices.filter(isSwitchDevice).map(toDeviceRow))

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
  if (!spaceId.value) { loading.value = false; inited.value = true; return }
  if (showLoading) loading.value = true
  loadError.value = ''
  const res = await getDevices(spaceId.value)
  if (res.error) loadError.value = res.error.message
  else setDevices(res.data || [])
  loading.value = false
  inited.value = true
  uni.stopPullDownRefresh()
}

/* 壳层协议；extra.space_id 来自告警详情/教室详情的跳转 */
function onEnter(extra) {
  if (!requireAuth()) return
  if (extra && extra.space_id && extra.space_id !== spaceId.value) {
    spaceId.value = extra.space_id
    store.devices = []
  }
  if (spaceId.value) connectWs(spaceId.value)
  refresh(!inited.value || (extra && extra.space_id))
}
function onRefresh() { refresh() }

function switchSpace(id) {
  if (spaceId.value === id) return
  spaceId.value = id
  store.devices = []
  connectWs(id)
  refresh(true)
}

async function onToggle({ device }) {
  const raw = device._raw || device
  const command = raw.app === 'on' ? 'off' : 'on'
  const { error } = await execDeviceCommand(raw, command)
  if (error) toastError(error)
}
function onOfflineTap() {
  uni.showToast({ title: '设备离线，暂时无法控制', icon: 'none' })
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
  gotoTab('overview')
}

defineExpose({ onEnter, onRefresh })
</script>

<style lang="scss" scoped>
.rv { padding-bottom: $sp-16; }

.rv__chips {
  @include flex(flex-start, center);
  flex-wrap: wrap;
  gap: $sp-16;
}
.rv__chip {
  padding: 12rpx 30rpx;
  border-radius: $r-tag;
  background: $bg-card;
  border: 1rpx solid $line;
  box-shadow: $shadow-card;
  font-size: $fs-aux;
  color: $text-2;
}
.rv__chip--on { background: $primary; color: $bg-card; font-weight: 600; box-shadow: $shadow-main-btn; }

.rv__empty-line {
  text-align: center;
  color: $text-3;
  font-size: $fs-aux;
  padding: 26rpx 0 8rpx;
}

/* 高风险操作区：位置靠底 + 红色警示 + 明确文案（三重区分） */
.rv__danger {
  margin: 40rpx $page-pad $sp-20;
  border: $danger-dash;
  border-radius: $r-card;
  padding: 30rpx;
  background: $bg-card;
}
.rv__danger-title {
  @include flex(flex-start, center);
  gap: $sp-12;
  color: $danger;
  font-size: $fs-card-title;
  font-weight: 700;
}
.rv__danger-sub {
  color: $text-3;
  font-size: $fs-tag;
  margin: 10rpx 0 26rpx;
  line-height: 36rpx;
}
.rv__danger-btn {
  display: flex;
  @include flex(center, center);
  gap: $sp-12;
  margin: 0 0 $sp-20;
}
.rv__danger-btn:last-child { margin-bottom: 0; }
</style>
