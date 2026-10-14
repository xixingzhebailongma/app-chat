<template>
  <view class="ac" :class="['ac--' + alarm.level, { 'is-pending': alarm.status === 'pending' }]" @tap="goDetail">
    <view class="ac__icon" :class="'ac__icon--' + alarm.level">{{ meta.icon }}</view>
    <view class="ac__body">
      <view class="ac__row1">
        <text class="ac__title">{{ alarm.title }}</text>
        <view class="tag" :class="statusMeta.cls">{{ statusMeta.label }}</view>
      </view>
      <view class="ac__row2">
        <text class="ac__space">{{ alarm.space_name || '' }}</text>
        <text class="ac__time">{{ timeText }}</text>
      </view>
      <view class="ac__row3">
        <text class="ac__summary">{{ alarm.summary }}</text>
      </view>
      <!-- 推送分级：如实标注被抑制 / 降级原因，解释"为什么没收到通知" -->
      <view v-if="alarm.suppress_reason || alarm.degrade_note" class="ac__note">
        <text>ℹ️ {{ alarm.suppress_reason || alarm.degrade_note }}</text>
      </view>
    </view>
  </view>
</template>

<script setup>
/**
 * 告警卡片：类型图标 + 教室名 + 简述 + 发生时间 + 状态标签 + 抑制/降级原因
 */
import { computed } from 'vue'
import { store } from '../store'
import { fromNow } from '../utils/time'

const props = defineProps({
  alarm: { type: Object, required: true },
})

const TYPE_META = {
  sensor_threshold: { icon: '🌡️', label: '环境告警' },
  device_offline: { icon: '📶', label: '设备离线' },
  security: { icon: '🚨', label: '安全告警' },
  system: { icon: '⚙️', label: '系统事件' },
}
const STATUS_META = {
  pending: { label: '未处理', cls: 'tag--red' },
  processing: { label: '处理中', cls: 'tag--orange' },
  resolved: { label: '已解决', cls: 'tag--green' },
}

const meta = computed(() => TYPE_META[props.alarm.type] || { icon: '🔔', label: '告警' })
const statusMeta = computed(() => STATUS_META[props.alarm.status] || { label: props.alarm.status, cls: 'tag--gray' })
const timeText = computed(() => {
  void store.tick
  return fromNow(props.alarm.occurred_at)
})

function goDetail() {
  uni.navigateTo({ url: '/pages/alarm/detail?id=' + props.alarm.id })
}
</script>

<style lang="scss" scoped>
.ac {
  display: flex;
  background: $card;
  border-radius: $radius-sm;
  padding: 24rpx;
  margin-bottom: 16rpx;
  border: 2rpx solid transparent;
}
/* 未处理告警整卡强调（不只靠小红点） */
.ac.is-pending { border-color: rgba(239, 68, 68, 0.45); background: #FFFAFA; }
.ac__icon {
  width: 80rpx;
  height: 80rpx;
  border-radius: 20rpx;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 40rpx;
  margin-right: 20rpx;
  flex-shrink: 0;
  background: #F1F4F9;
}
.ac__icon--critical { background: $danger-light; }
.ac__icon--warning { background: $warning-light; }
.ac__icon--info { background: #EDF1F7; }
.ac__body { flex: 1; min-width: 0; }
.ac__row1 { display: flex; align-items: center; justify-content: space-between; gap: 12rpx; }
.ac__title { font-size: $fs-md; font-weight: 600; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.ac__row2 { display: flex; align-items: center; gap: 16rpx; margin-top: 8rpx; }
.ac__space { font-size: $fs-xs; color: $text-2; }
.ac__time { font-size: $fs-xs; color: $text-3; }
.ac__row3 { margin-top: 6rpx; }
.ac__summary { font-size: $fs-sm; color: $text-2; }
.ac__note {
  margin-top: 12rpx;
  background: #F6F8FB;
  border-radius: 10rpx;
  padding: 10rpx 16rpx;
  font-size: $fs-xs;
  color: $text-3;
  line-height: 34rpx;
}
</style>
