<template>
  <view
    class="ac"
    :class="'ac--' + alarm.status"
    hover-class="ac--press"
    :hover-stay-time="60"
    @tap="$emit('click', alarm)"
  >
    <!-- 左侧 8rpx 状态色条（圆角随卡片，靠 overflow 裁剪） -->
    <view class="ac__bar" />

    <view class="ac__body">
      <!-- 第一行：类型图标 + 标题 + 状态胶囊 -->
      <view class="ac__row1">
        <view class="ac__typeicon">
          <app-icon :name="iconName" :size="40" :color="meta.color" />
        </view>
        <text class="ac__title">{{ alarm.title }}</text>
        <text class="tag ac__status" :class="meta.tagClass">{{ meta.text }}</text>
      </view>

      <!-- 第二行：教室名 · 简述（最多两行） -->
      <view class="ac__row2">
        <text class="ac__room">{{ alarm.room }}</text>
        <text class="ac__dot-pad">·</text>
        <text class="ac__desc">{{ alarm.desc }}</text>
      </view>

      <!-- 第三行：时间 + 说明胶囊（已合并/冷却中/短信已送达） -->
      <view class="ac__row3">
        <text class="ac__time num">{{ alarm.time }}</text>
        <view v-if="notes.length" class="ac__notes">
          <text
            v-for="(n, i) in notes"
            :key="i"
            class="ac__note"
            :class="'ac__note--' + (n.tone || 'gray')"
          >{{ n.text }}</text>
        </view>
      </view>
    </view>
  </view>
</template>

<script setup>
import { computed } from 'vue'
import { COLORS } from '../../config/theme'

/**
 * 告警卡片
 * @prop alarm {
 *   status: unhandled | processing | resolved,
 *   type(图标名，默认 warning), title, room, desc, time,
 *   notes: [{ text, tone: gray|orange|blue }]
 * }
 */
const props = defineProps({
  alarm: { type: Object, required: true },
})
defineEmits(['click'])

const STATUS_MAP = {
  unhandled:  { text: '未处理', color: COLORS.danger,  tagClass: 'tag--red' },
  processing: { text: '处理中', color: COLORS.warning, tagClass: 'tag--orange' },
  resolved:   { text: '已解决', color: COLORS.text3,   tagClass: 'tag--gray' },
}
const meta = computed(() => STATUS_MAP[props.alarm.status] || STATUS_MAP.unhandled)
const iconName = computed(() => props.alarm.type || 'warning')
const notes = computed(() => props.alarm.notes || [])
</script>

<style lang="scss" scoped>
.ac {
  position: relative;
  display: flex;
  background: $bg-card;
  border-radius: $r-card;
  border: 1rpx solid $line;
  box-shadow: $shadow-card;
  margin-bottom: $sp-20;
  overflow: hidden;
  transition: transform $t-micro $ease-standard, opacity $t-micro $ease-standard;
}
.ac--press { transform: scale(0.98); opacity: 0.92; }

/* 未处理：左侧浅红光晕（无描边语言下仍保留警示层级） */
.ac--unhandled {
  box-shadow: $shadow-card, $glow-danger;
}

.ac__bar {
  width: 8rpx;
  flex: none;
}
.ac--unhandled  .ac__bar { background: $danger; }
.ac--processing .ac__bar { background: $warning; }
.ac--resolved   .ac__bar { background: $text-4; }

.ac__body {
  flex: 1;
  min-width: 0;
  padding: $sp-24 $sp-24 $sp-20;
}

.ac__row1 {
  @include flex(flex-start, center);
  gap: $sp-12;
}
.ac__typeicon { flex: none; @include flex(center, center); }
.ac__title {
  flex: 1;
  min-width: 0;
  font-size: $fs-card-title;
  font-weight: 600;
  color: $text-1;
  @include ellipsis(1);
}
.ac__status { flex: none; }

.ac__row2 {
  margin-top: $sp-12;
  font-size: $fs-body-sm;
  color: $text-2;
  line-height: 1.5;
}
.ac__room { color: $text-1; }
.ac__dot-pad { margin: 0 8rpx; color: $text-4; }
.ac__desc {
  @include ellipsis(2);
}

.ac__row3 {
  margin-top: $sp-12;
  @include flex(flex-start, center);
  gap: $sp-12;
  flex-wrap: wrap;
}
.ac__time { font-size: $fs-aux; color: $text-3; }

.ac__notes { @include flex(flex-start, center); gap: 8rpx; flex-wrap: wrap; }
.ac__note {
  font-size: $fs-tag;
  line-height: 32rpx;
  padding: 0 12rpx;
  border-radius: $r-tag;
}
.ac__note--gray   { background: $disabled-light; color: $text-2; }
.ac__note--orange { background: $warning-light; color: $warning; }
.ac__note--blue   { background: $primary-light; color: $primary; }
</style>
