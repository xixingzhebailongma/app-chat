<template>
  <view v-if="readings.length" class="sp">
    <view v-for="r in readings" :key="r.key" class="sp__item" :class="'sp__item--' + r.level">
      <text class="sp__icon">{{ r.icon }}</text>
      <view class="sp__body">
        <view class="sp__value-row">
          <text class="sp__value">{{ fmt(r) }}</text>
          <text v-if="r.unit" class="sp__unit">{{ r.unit }}</text>
        </view>
        <text class="sp__label">{{ r.label }}</text>
      </view>
    </view>
  </view>
</template>

<script setup>
/**
 * 环境数据卡片区：只渲染设备 app JSON 里实际存在的字段，
 * 不支持的指标不占位、不显示"—"或 0。
 */
import { computed } from 'vue'
import { envReadings, fmtSensorValue } from '../utils/sensor'

const props = defineProps({
  devices: { type: Array, default: () => [] },
})

const readings = computed(() => envReadings(props.devices))

function fmt(r) { return fmtSensorValue(r) }
</script>

<style lang="scss" scoped>
.sp {
  display: flex;
  flex-wrap: wrap;
  gap: 16rpx;
}
.sp__item {
  width: calc((100% - 32rpx) / 3);
  box-sizing: border-box;
  background: $card;
  border-radius: $radius-sm;
  padding: 22rpx 20rpx;
  display: flex;
  align-items: center;
  border: 2rpx solid transparent;
}
.sp__item--ok { border-color: transparent; }
.sp__item--warn { border-color: rgba(245, 158, 11, 0.4); background: #FFFBF3; }
.sp__item--danger { border-color: rgba(239, 68, 68, 0.4); background: #FFF5F5; }
.sp__icon { font-size: 40rpx; margin-right: 14rpx; }
.sp__body { min-width: 0; }
.sp__value-row { display: flex; align-items: baseline; }
.sp__value { font-size: 34rpx; font-weight: 700; color: $text-1; }
.sp__item--warn .sp__value { color: $warning-deep; }
.sp__item--danger .sp__value { color: $danger-deep; }
.sp__unit { font-size: $fs-xs; color: $text-3; margin-left: 4rpx; }
.sp__label { display: block; font-size: $fs-xs; color: $text-3; margin-top: 4rpx; }
</style>
