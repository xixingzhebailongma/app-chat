<template>
  <view class="eg">
    <env-card
      v-for="r in readings"
      :key="r.key"
      :metric-key="metricKey(r.key)"
      :value="fmt(r)"
      :alarm="r.level !== 'ok'"
      :supported="true"
    />
    <view v-if="!readings.length" class="eg__empty">教室内暂无在线传感器</view>
  </view>
</template>

<script setup>
/**
 * 环境数据网格：只渲染设备 app JSON 里实际存在的字段，
 * 不支持的指标整卡不渲染、不占位、不显示"—"或 0。
 */
import { computed } from 'vue'
import { envReadings, fmtSensorValue } from '../../utils/sensor'

const props = defineProps({
  devices: { type: Array, default: () => [] },
})

const readings = computed(() => envReadings(props.devices))

/** SENSOR_META 的 pir 对应 METRIC_THEME 的 motion */
function metricKey(k) {
  return k === 'pir' ? 'motion' : k
}
function fmt(r) {
  return fmtSensorValue(r)
}
</script>

<style lang="scss" scoped>
.eg {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: $sp-16;
}
.eg__empty {
  grid-column: 1 / -1;
  text-align: center;
  color: $text-3;
  font-size: $fs-aux;
  padding: 30rpx 0 10rpx;
}
</style>
