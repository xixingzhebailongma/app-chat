<template>
  <view class="dc" :class="{ 'is-offline': offline }" @tap="onCardTap">
    <view class="dc__main">
      <view class="dc__icon" :class="{ 'dc__icon--on': isOn && !offline }">{{ icon }}</view>
      <view class="dc__info">
        <view class="dc__name-row">
          <text class="dc__name">{{ name }}</text>
          <view v-if="offline" class="tag tag--gray">设备离线</view>
          <view v-else-if="pending" class="tag tag--blue">指令下发中</view>
          <view v-else-if="confirmed" class="tag tag--orange">执行确认中</view>
        </view>
        <text class="dc__sub">{{ subText }}</text>
      </view>
    </view>

    <view class="dc__side">
      <!-- 开关类：三段式确认（pending 显示加载态，不做乐观更新） -->
      <view v-if="toggleable" class="switch" :class="{ 'switch--on': isOn, 'switch--disabled': offline || pending }" @tap.stop="onToggle">
        <view v-if="pending" class="switch__spinner"></view>
        <view v-else class="switch__knob"></view>
      </view>
      <view v-else class="tag" :class="offline ? 'tag--gray' : 'tag--green'">
        <view class="dot" :class="offline ? 'dot--gray' : 'dot--green'" style="margin-right:8rpx;"></view>
        {{ offline ? '离线' : '在线' }}
      </view>
    </view>
  </view>
</template>

<script setup>
/**
 * 设备卡片：
 * - status（网络连接）与 app（开关状态）是两个独立维度，分开渲染：
 *   status='online' 且 app='off' 是合法状态（在线但开关关闭）
 * - 设备离线：开关置灰不可点，提示"设备离线"
 * - pending：指令下发中，开关加载态，不做乐观更新
 */
import { computed } from 'vue'
import { store } from '../store'
import { iconOf, typeName, isOffline } from '../utils/device'
import { fromNow } from '../utils/time'

const props = defineProps({
  device: { type: Object, required: true },
  toggleable: { type: Boolean, default: false },
  showLastSeen: { type: Boolean, default: true },
})
const emit = defineEmits(['toggle'])

const d = computed(() => props.device || {})
const offline = computed(() => isOffline(d.value))
const isOn = computed(() => d.value.app === 'on')
const icon = computed(() => iconOf(d.value))
const name = computed(() => d.value.label || d.value.device_id)
const pendingCmd = computed(() => store.pendingCmds[d.value.device_id])
const pending = computed(() => !!pendingCmd.value && pendingCmd.value.phase === 'sent')
const confirmed = computed(() => !!pendingCmd.value && pendingCmd.value.phase === 'confirmed')

const subText = computed(() => {
  void store.tick // 依赖 tick 刷新相对时间
  const parts = [typeName(d.value)]
  if (props.showLastSeen && d.value.last_seen) parts.push('上报 ' + fromNow(d.value.last_seen))
  return parts.join(' · ')
})

function onToggle() {
  if (offline.value || pending.value) return
  emit('toggle', d.value)
}
function onCardTap() {
  if (offline.value) {
    uni.showToast({ title: '设备离线，暂时无法控制', icon: 'none' })
  }
}
</script>

<style lang="scss" scoped>
.dc {
  display: flex;
  align-items: center;
  justify-content: space-between;
  background: $card;
  border-radius: $radius-sm;
  padding: 24rpx;
  margin-bottom: 16rpx;
  transition: opacity 0.2s;
  min-height: 120rpx;
}
.is-offline { opacity: 0.55; }
.dc__main { display: flex; align-items: center; flex: 1; min-width: 0; }
.dc__icon {
  width: 84rpx;
  height: 84rpx;
  border-radius: 20rpx;
  background: #F1F4F9;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 42rpx;
  margin-right: 20rpx;
  flex-shrink: 0;
}
.dc__icon--on { background: $primary-light; }
.dc__info { flex: 1; min-width: 0; }
.dc__name-row { display: flex; align-items: center; flex-wrap: wrap; gap: 10rpx; }
.dc__name { font-size: $fs-md; font-weight: 600; max-width: 320rpx; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.dc__sub { display: block; font-size: $fs-xs; color: $text-3; margin-top: 6rpx; }

/* 自定义开关（热区 ≥ 44px） */
.switch {
  width: 104rpx;
  height: 60rpx;
  border-radius: 30rpx;
  background: #DDE3EC;
  position: relative;
  transition: background 0.25s;
  flex-shrink: 0;
}
.switch--on { background: $success; }
.switch--disabled { opacity: 0.6; }
.switch__knob {
  position: absolute;
  top: 4rpx;
  left: 4rpx;
  width: 52rpx;
  height: 52rpx;
  border-radius: 50%;
  background: #fff;
  box-shadow: 0 2rpx 8rpx rgba(0, 0, 0, 0.18);
  transition: transform 0.25s;
}
.switch--on .switch__knob { transform: translateX(44rpx); }
.switch__spinner {
  position: absolute;
  top: 50%;
  left: 50%;
  width: 36rpx;
  height: 36rpx;
  margin: -18rpx 0 0 -18rpx;
  border: 5rpx solid rgba(255, 255, 255, 0.45);
  border-top-color: #fff;
  border-radius: 50%;
  animation: spin 0.8s linear infinite;
}
@keyframes spin { to { transform: rotate(360deg); } }
</style>
