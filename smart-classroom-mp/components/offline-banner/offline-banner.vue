<template>
  <view v-if="banner" class="ob" :class="'ob--' + banner.kind">
    <view class="ob__dot" :class="['ob__dot--' + banner.tone, { 'sc-breathe': banner.breathe }]" />
    <text class="ob__text">{{ banner.text }}</text>
    <text v-if="banner.kind === 'ws'" class="ob__retry" hover-class="press" @tap="retry">重连</text>
  </view>
</template>

<script setup>
/**
 * 离线态横幅（内容顶部 inline，不遮挡数据）：
 * - 网络断开：明确提示 + 保留上次数据（不清空界面）
 * - 实时连接断开：提示重连中，数据仍展示最近快照
 */
import { computed } from 'vue'
import { store } from '../../store'
import { connectWs, ensureWs } from '../../services/ws'

const props = defineProps({
  // 是否关注 WS 状态（实时页面传 true）
  ws: { type: Boolean, default: true },
})

const banner = computed(() => {
  if (!store.netOnline) {
    return { kind: 'net', tone: 'red', text: '网络已断开，当前展示最近数据', breathe: false }
  }
  if (props.ws) {
    if (store.wsStatus === 'connecting') {
      return { kind: 'ws', tone: 'orange', text: '正在建立实时连接…', breathe: true }
    }
    if (store.wsStatus === 'reconnecting') {
      return { kind: 'ws', tone: 'orange', text: '实时连接中断，正在自动重连…', breathe: true }
    }
    if (store.wsStatus === 'offline') {
      return { kind: 'ws', tone: 'gray', text: '实时连接已断开，点击重连', breathe: false }
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
  @include flex(flex-start, center);
  margin: $sp-16 $page-pad 0;
  padding: 16rpx $sp-24;
  border-radius: $r-icon;
  font-size: $fs-aux;
  animation: sc-slide-down $t-toggle $ease-standard both;
}
.ob--net { background: $danger-light; color: $danger; }
.ob--ws { background: $warning-light; color: $warning; }
.ob__dot {
  width: 14rpx;
  height: 14rpx;
  border-radius: 50%;
  flex: none;
}
.ob__dot--red { background: $danger; }
.ob__dot--orange { background: $warning; }
.ob__dot--gray { background: $disabled; }
.ob__text { flex: 1; margin-left: $sp-12; }
.ob__retry { font-weight: 600; padding: 8rpx 10rpx; }
</style>
