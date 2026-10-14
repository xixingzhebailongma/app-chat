<template>
  <view
    class="dr"
    :class="{ 'dr--card': card, 'dr--off': !device.online }"
    hover-class="dr--press"
    :hover-stay-time="60"
    @tap="onRowTap"
  >
    <view class="dr__icon" :class="{ 'dr__icon--off': !device.online }" :style="iconStyle">
      <app-icon :name="icon" :size="40" :color="device.online ? catColor : COLORS.text3" />
    </view>

    <view class="dr__main">
      <text class="dr__name">{{ name }}</text>
      <text class="dr__sub">{{ subText }}</text>
    </view>

    <view v-if="controllable" class="dr__action" @tap.stop>
      <ui-switch
        :value="isOn"
        :pending="!!device.pending"
        :disabled="!device.online"
        @switch="onSwitch"
      />
    </view>
    <!-- 传感器类：右侧实时小读数（固定宽度） -->
    <view v-else class="dr__reading">
      <slot name="right">
        <text class="dr__reading-num num">{{ device.reading }}</text>
        <text v-if="device.unit" class="dr__reading-unit">{{ device.unit }}</text>
      </slot>
    </view>
  </view>
</template>

<script setup>
import { computed } from 'vue'
import { deviceIcon, deviceCat } from '../../config/metrics.js'
import { COLORS } from '../../config/theme'

/**
 * 设备列表项（高 ≥120rpx）
 * @prop device {
 *   device_id, device_type, label|name, online, state('on'|'off')|on,
 *   pending, sub(副信息，如“3 分钟前上报”), reading/unit(传感器读数)
 * }
 * @prop card         独立白卡样式；false 时为分组大卡内的细分割行
 * @prop controllable 是否为开关类设备；false 时右侧渲染读数
 */
const props = defineProps({
  device: { type: Object, required: true },
  card: { type: Boolean, default: true },
  controllable: { type: Boolean, default: true },
})
const emit = defineEmits(['toggle', 'offline-tap', 'click'])

const name = computed(() => props.device.label || props.device.name || props.device.device_id)
const icon = computed(() => deviceIcon(props.device.iconType || props.device.device_type))
const isOn = computed(() => props.device.state === 'on' || props.device.on === true)
/* 在线：分类点缀色图标 + 成对浅底；离线：整块置灰（class 控制） */
const catColor = computed(() => deviceCat(icon.value).color)
const iconStyle = computed(() =>
  props.device.online ? { background: deviceCat(icon.value).soft } : {}
)
const subText = computed(() => {
  if (props.device.sub !== undefined) return props.device.sub
  return props.device.online ? '' : '设备已离线'
})

function onRowTap() {
  if (!props.device.online) {
    emit('offline-tap', props.device) // 页面 toast：设备离线，暂不可控
    return
  }
  emit('click', props.device)
}
function onSwitch(next) {
  if (!props.device.online) {
    emit('offline-tap', props.device)
    return
  }
  emit('toggle', { device: props.device, next })
}
</script>

<style lang="scss" scoped>
.dr {
  @include flex(flex-start, center);
  min-height: 120rpx;
  gap: $sp-20;
  transition: opacity $t-toggle $ease-standard;
}
.dr--card {
  background: $bg-card;
  border-radius: $r-card;
  border: 1rpx solid $line;
  box-shadow: $shadow-card;
  padding: $sp-20 $sp-24;
  margin-bottom: $sp-20;
}
.dr--press { opacity: 0.75; }

/* 分组大卡内的行：底部分割细线，最后一条由父级 :last-child 去除 */
.dr:not(.dr--card) {
  padding: $sp-20 0;
  border-bottom: 1rpx solid $line;
}

.dr__icon {
  width: 72rpx;
  height: 72rpx;
  border-radius: $r-icon;
  @include flex(center, center);
  flex: none;
  transition: background-color $t-toggle $ease-standard;
}
.dr__icon--off { background: $disabled-light; }

.dr__main {
  flex: 1;
  min-width: 0;
  @include flex(center, flex-start, column);
  gap: 8rpx;
}
.dr__name {
  font-size: $fs-body;
  color: $text-1;
  @include ellipsis(1);
}
.dr__sub {
  font-size: $fs-aux;
  color: $text-3;
  @include ellipsis(1);
}

.dr--off .dr__name { color: $text-3; }

.dr__action { flex: none; }

.dr__reading {
  flex: none;
  min-width: 160rpx;
  text-align: right;
  @include flex(flex-end, baseline);
  gap: 6rpx;
}
.dr__reading-num {
  font-size: $fs-card-title;
  font-weight: 600;
  color: $text-1;
}
.dr__reading-unit { font-size: $fs-tag; color: $text-3; }
.dr--off .dr__reading-num { color: $text-3; }
</style>
