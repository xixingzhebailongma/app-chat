<template>
  <view v-if="render" class="cm">
    <!-- 遮罩：rgba(0,0,0,.45) 淡入，280ms -->
    <view class="cm__mask" :class="{ 'cm__mask--in': entering }" @tap="onMask" />

    <view class="cm__anchor">
      <view
        class="cm__panel"
        :class="{ 'cm__panel--in': entering }"
        @tap.stop
      >
        <!-- 危险警示头部图标（红色） -->
        <view v-if="headerIcon" class="cm__header">
          <view class="cm__header-icon" :class="{ 'cm__header-icon--danger': danger }">
            <app-icon :name="headerIcon" :size="52" :color="danger ? COLORS.danger : COLORS.primary" />
          </view>
        </view>

        <text class="cm__title">{{ title }}</text>
        <text v-if="content" class="cm__content">{{ content }}</text>

        <!-- 场景确认：小清单列出将影响的设备（每台带图标） -->
        <slot />

        <view class="cm__btns">
          <view
            class="cm__btn cm__btn--cancel"
            hover-class="press"
            :hover-stay-time="60"
            @tap="$emit('cancel')"
          >{{ cancelText }}</view>
          <view
            class="cm__btn"
            :class="danger ? 'cm__btn--danger' : 'cm__btn--primary'"
            hover-class="press"
            :hover-stay-time="60"
            @tap="$emit('confirm')"
          >{{ confirmText }}</view>
        </view>
      </view>
    </view>
  </view>
</template>

<script setup>
/**
 * 全局统一二次确认弹窗（禁止使用 window.alert/confirm 与原生 showModal）
 * @prop visible      显隐
 * @prop danger       危险操作：红色主按钮（文案必须写明后果，如“强制开门”）
 * @prop headerIcon   顶部图标名（危险场景传 'warning'）
 * @prop maskClosable 点遮罩是否可取消
 * 默认插槽：影响设备清单等自定义内容
 */
import { ref, watch, nextTick } from 'vue'
import { COLORS } from '../../config/theme'

const props = defineProps({
  visible: { type: Boolean, default: false },
  title: { type: String, default: '提示' },
  content: { type: String, default: '' },
  confirmText: { type: String, default: '确认' },
  cancelText: { type: String, default: '取消' },
  danger: { type: Boolean, default: false },
  headerIcon: { type: String, default: '' },
  maskClosable: { type: Boolean, default: true },
})
const emit = defineEmits(['confirm', 'cancel'])

const render = ref(false)
const entering = ref(false)
let leaveTimer = null

watch(
  () => props.visible,
  (v) => {
    if (v) {
      clearTimeout(leaveTimer)
      render.value = true
      entering.value = false
      nextTick(() => { entering.value = true })
    } else {
      entering.value = false
      leaveTimer = setTimeout(() => { render.value = false }, 280)
    }
  },
  { immediate: true }
)

function onMask() {
  // 危险确认由父级显式传 :mask-closable="false"，防止误触关闭
  if (props.maskClosable) emit('cancel')
}
</script>

<style lang="scss" scoped>
.cm {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  z-index: 1000;
}
.cm__mask {
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: $mask;
  opacity: 0;
  transition: opacity $t-page $ease-standard;
}

.cm__anchor {
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  @include flex(center, center);
  padding: 0 $sp-40;
}
.cm__panel {
  width: 100%;
  max-width: 620rpx;
  background: $bg-card;
  border-radius: $r-modal;
  padding: $sp-32 $sp-28 $sp-28;
  opacity: 0;
  transform: scale(0.92);
  transition: opacity $t-page $ease-standard, transform $t-page $ease-standard;
}
.cm__panel--in { opacity: 1; transform: scale(1); }

.cm__header { @include flex(center, center); margin-bottom: $sp-16; }
.cm__header-icon {
  width: 96rpx;
  height: 96rpx;
  border-radius: 50%;
  background: $primary-light;
  @include flex(center, center);
}
.cm__header-icon--danger { background: $danger-light; }

.cm__title {
  display: block;
  text-align: center;
  font-size: $fs-card-title;
  font-weight: 600;
  color: $text-1;
}
.cm__content {
  display: block;
  margin-top: $sp-12;
  font-size: $fs-body;
  color: $text-2;
  line-height: 1.6;
  text-align: center;
}

.cm__btns {
  margin-top: $sp-28;
  @include flex(space-between, stretch, row);
  gap: $sp-20;
}
.cm__btn {
  flex: 1;
  height: 88rpx;
  border-radius: $r-btn;
  @include flex(center, center);
  font-size: 30rpx;
  transition: opacity $t-micro $ease-standard;
}
.cm__btn--cancel { background: $disabled-light; color: $text-2; }
.cm__btn--primary {
  background: $primary;
  color: #fff;
}
.cm__btn--danger { background: $danger; color: #fff; }
</style>
