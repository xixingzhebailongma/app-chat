<template>
  <!-- 设备不支持的指标：整张卡不渲染，不占位、不显示“—” -->
  <view v-if="supported" class="env-card" hover-class="env-card--press" :hover-stay-time="60">
    <view v-if="alarm" class="env-card__warn">
      <app-icon name="warning" :size="28" :color="COLORS.danger" />
    </view>

    <view class="env-card__head">
      <view class="env-card__icon" :style="{ background: softBg }">
        <app-icon :name="theme.icon" :size="36" :color="theme.color" />
      </view>
      <text class="env-card__label">{{ label || theme.label }}</text>
    </view>

    <view class="env-card__value">
      <!-- 数值变化时 600ms 浅蓝高亮闪动；固定宽度 + 等宽数字防抖动 -->
      <text
        class="env-card__num num"
        :class="{ 'env-card__num--alarm': alarm, 'env-card__num--flash': flashing }"
      >{{ value }}</text>
      <text v-if="unitText" class="env-card__unit">{{ unitText }}</text>
    </view>
  </view>
</template>

<script setup>
import { computed, ref, watch, nextTick } from 'vue'
import { METRIC_THEME } from '../../config/metrics.js'
import { COLORS } from '../../config/theme'

/**
 * 环境指标卡（2 列网格单元）
 * @prop metricKey temperature | humidity | co2 | light | pm25 | motion
 * @prop value     当前读数
 * @prop unit      覆盖单位
 * @prop label     覆盖名称
 * @prop alarm     是否超阈值
 * @prop supported 是否支持（false 时整卡不渲染）
 */
const props = defineProps({
  metricKey: { type: String, required: true },
  value: { type: [String, Number], default: '--' },
  unit: { type: String, default: '' },
  label: { type: String, default: '' },
  alarm: { type: Boolean, default: false },
  supported: { type: Boolean, default: true },
})

const theme = computed(() => METRIC_THEME[props.metricKey] || METRIC_THEME.temperature)
const unitText = computed(() => (props.unit === '' ? theme.value.unit : props.unit))
const softBg = computed(() => theme.value.soft)

// 数值变化闪动（先移除 class，下一帧重新挂载以重启动画）
const flashing = ref(false)
let flashTimer = null
watch(() => props.value, () => {
  flashing.value = false
  nextTick(() => {
    flashing.value = true
    clearTimeout(flashTimer)
    flashTimer = setTimeout(() => { flashing.value = false }, 600)
  })
})
</script>

<style lang="scss" scoped>
.env-card {
  position: relative;
  background: $bg-card;
  border-radius: $r-card;
  border: 1rpx solid $line;
  box-shadow: $shadow-card;
  padding: $sp-24;
  transition: transform $t-micro $ease-standard, opacity $t-micro $ease-standard;
}
.env-card--press { transform: scale(0.97); }

.env-card__head {
  @include flex(flex-start, center);
  gap: $sp-12;
  margin-bottom: $sp-16;
}
.env-card__icon {
  width: 64rpx;
  height: 64rpx;
  border-radius: $r-icon;
  @include flex(center, center);
  flex: none;
}
.env-card__label {
  font-size: $fs-body;
  color: $text-2;
  @include ellipsis(1);
}

.env-card__value {
  @include flex(flex-start, flex-end);
  gap: 8rpx;
}
.env-card__num {
  /* 固定最小宽度，实时刷新不宽窄变化；DIN 等宽数字 */
  min-width: 128rpx;
  font-size: $fs-num;
  font-weight: 700;
  line-height: 1.1;
  color: $text-1;
  border-radius: $r-tag;
}
.env-card__unit {
  font-size: $fs-unit;
  color: $text-3;
  line-height: 1.4;
}
.env-card__num--alarm { color: $danger; }
.env-card__num--flash { animation: sc-num-flash $t-flash $ease-standard; }

.env-card__warn {
  position: absolute;
  top: $sp-16;
  right: $sp-16;
}
</style>
