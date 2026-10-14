<template>
  <view v-if="banner" class="ob" :class="'ob--' + banner.kind">
    <view class="dot" :class="banner.dotClass"></view>
    <text class="ob__text">{{ banner.text }}</text>
    <text v-if="banner.kind === 'ws'" class="ob__retry" @tap="retry">重连</text>
  </view>
</template>

<script setup>
/**
 * 离线态横幅：
 * - 网络断开：明确提示 + 保留上次数据（不清空界面）
 * - 实时连接断开：提示重连中，数据仍展示最近快照
 */
import { computed } from 'vue'
import { store } from '../store'
import { connectWs, ensureWs } from '../services/ws'

const props = defineProps({
  // 是否关注 WS 状态（实时页面传 true）
  ws: { type: Boolean, default: true },
})

const banner = computed(() => {
  if (!store.netOnline) {
    return { kind: 'net', text: '网络已断开，当前展示最近数据', dotClass: 'dot--red' }
  }
  if (props.ws) {
    if (store.wsStatus === 'connecting') {
      return { kind: 'ws', text: '正在建立实时连接…', dotClass: 'dot--orange' }
    }
    if (store.wsStatus === 'reconnecting') {
      return { kind: 'ws', text: '实时连接中断，正在自动重连…', dotClass: 'dot--orange' }
    }
    if (store.wsStatus === 'offline') {
      return { kind: 'ws', text: '实时连接已断开，点击重连', dotClass: 'dot--gray' }
    }
  }
  return null
})

function retry() {
  ensureWs()
  if (store.wsStatus === 'offline' || store.wsStatus === 'reconnecting') {
    connectWs(store.spaceId)
  }
}
</script>

<style lang="scss" scoped>
.ob {
  display: flex;
  align-items: center;
  margin: 20rpx $gap 0;
  padding: 18rpx 24rpx;
  border-radius: $radius-sm;
  font-size: $fs-sm;
}
.ob--net { background: $danger-light; color: $danger-deep; }
.ob--ws { background: $warning-light; color: $warning-deep; }
.ob__text { flex: 1; margin-left: 16rpx; }
.ob__retry { font-weight: 600; padding: 0 10rpx; }
</style>
