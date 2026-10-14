<template>
  <view class="page">
    <offline-banner :ws="false" />

    <state-view
      :loading="loading"
      :error="loadError"
      :empty="!loading && !loadError && !alarm"
      empty-text="告警不存在或已删除"
      empty-desc="可能已被其他管理员处理，请返回列表刷新"
      empty-action="返回列表"
      empty-icon="bell"
      @retry="load"
      @empty-action="goBack"
    >
      <!-- 告警头部 -->
      <view class="head" :class="'head--' + alarm.level">
        <view class="head__row">
          <view class="head__icon" :class="'head__icon--' + alarm.level">
            <app-icon :name="meta.icon" :size="44" :color="meta.color" />
          </view>
          <view class="head__main">
            <text class="head__title">{{ alarm.title }}</text>
            <view class="head__tags">
              <view class="tag" :class="statusMeta.cls">{{ statusMeta.label }}</view>
              <view class="tag tag--gray">{{ meta.label }}</view>
            </view>
          </view>
        </view>
        <view class="head__summary">{{ alarm.summary }}</view>
        <view class="head__meta">
          <view class="head__meta-item">
            <app-icon name="room" :size="26" :color="COLORS.text3" />
            <text>{{ alarm.space_name }}</text>
          </view>
          <view class="head__meta-item">
            <app-icon name="clock" :size="26" :color="COLORS.text3" />
            <text>{{ occurredText }}</text>
          </view>
        </view>
        <!-- 抑制 / 降级原因说明 -->
        <view v-if="alarm.suppress_reason || alarm.degrade_note" class="head__note">
          {{ alarm.suppress_reason || alarm.degrade_note }}
        </view>
      </view>

      <!-- 完整时间线：发现 → 推送 → 处理 -->
      <view class="card">
        <view class="card__title">处理时间线</view>
        <view class="tl">
          <view v-for="(t, i) in timeline" :key="i" class="tl__item">
            <view class="tl__rail">
              <view class="tl__dot" :class="'tl__dot--' + i"></view>
              <view v-if="i < timeline.length - 1" class="tl__line"></view>
            </view>
            <view class="tl__body">
              <view class="tl__row">
                <text class="tl__stage">{{ t.stage }}</text>
                <text class="tl__time">{{ timeText(t.time) }}</text>
              </view>
              <text class="tl__note">{{ t.note }}</text>
              <text v-if="t.by" class="tl__by">操作人：{{ t.by }}</text>
            </view>
          </view>
        </view>
      </view>
    </state-view>

    <!-- 管理员：底部固定操作栏（教师无「标记已处理」按钮） -->
    <view v-if="canHandle && alarm" class="action-bar">
      <button class="c-btn c-btn--ghost c-btn--sm" @tap="openNote">添加备注</button>
      <button class="c-btn c-btn--ghost c-btn--sm" @tap="goRemote">远程控制</button>
      <button
        v-if="alarm.status !== 'resolved'"
        class="c-btn c-btn--primary c-btn--sm action-bar__main"
        @tap="markResolved"
      >标记已处理</button>
      <button v-else class="c-btn c-btn--ghost c-btn--sm action-bar__main" disabled>已解决</button>
    </view>

    <!-- 备注弹层（跨端一致，替代 wx.showModal editable） -->
    <view v-if="noteVisible" class="mask" @tap="noteVisible = false">
      <view class="popup" @tap.stop>
        <text class="popup__title">添加处理备注</text>
        <textarea
          v-model="noteText"
          class="popup__textarea"
          placeholder="记录处理措施、现场情况等（将写入运维日志）"
          :maxlength="200"
        />
        <view class="popup__btns">
          <button class="c-btn c-btn--ghost c-btn--sm" @tap="noteVisible = false">取消</button>
          <button class="c-btn c-btn--primary c-btn--sm" :disabled="saving" @tap="saveNote">
            {{ saving ? '保存中…' : '保存' }}
          </button>
        </view>
      </view>
    </view>
  </view>
</template>

<script setup>
/**
 * 告警详情：完整时间线（发现 → 推送 → 处理）
 * 管理员底部操作栏：[添加备注] [远程控制] [标记已处理]（每条操作留操作人和时间，形成运维日志）
 */
import { ref, computed } from 'vue'
import { onLoad, onShow } from '@dcloudio/uni-app'
import { getAlarmDetail, handleAlarm } from '../../api'
import { store } from '../../store'
import { toastError } from '../../api/request'
import { can, isAdmin, gotoTab } from '../../utils/permission'
import { fmtDateTime } from '../../utils/time'
import { COLORS } from '../../config/theme'

const TYPE_META = {
  sensor_threshold: { icon: 'temperature', label: '环境告警', color: COLORS.danger },
  device_offline: { icon: 'sensor', label: '设备离线', color: COLORS.warning },
  security: { icon: 'warning', label: '安全告警', color: COLORS.danger },
  system: { icon: 'info', label: '系统事件', color: COLORS.primary },
}
const STATUS_META = {
  pending: { label: '未处理', cls: 'tag--red' },
  processing: { label: '处理中', cls: 'tag--orange' },
  resolved: { label: '已解决', cls: 'tag--green' },
}

const id = ref('')
const loading = ref(true)
const loadError = ref('')
const alarm = ref(null)
const noteVisible = ref(false)
const noteText = ref('')
const saving = ref(false)

const meta = computed(() => (alarm.value && TYPE_META[alarm.value.type]) || { icon: 'warning', label: '告警', color: COLORS.danger })
const statusMeta = computed(() => (alarm.value && STATUS_META[alarm.value.status]) || { label: '-', cls: 'tag--gray' })
const timeline = computed(() => (alarm.value && alarm.value.timeline) || [])
const occurredText = computed(() => (alarm.value ? fmtDateTime(alarm.value.occurred_at) : ''))
const canHandle = computed(() => can('alarm:handle'))

async function load() {
  loading.value = true
  loadError.value = ''
  const res = await getAlarmDetail(id.value)
  if (res.error) loadError.value = res.error.message
  else alarm.value = res.data
  loading.value = false
}

onLoad((options) => { id.value = (options && options.id) || '' })
onShow(() => { if (id.value) load() })

function timeText(t) { return fmtDateTime(t) }

async function markResolved() {
  const { confirm } = await new Promise((resolve) => {
    uni.showModal({
      title: '标记已处理',
      content: '确认将此告警标记为已解决？操作将记录到运维日志。',
      confirmColor: COLORS.primary,
      success: (r) => resolve({ confirm: !!r.confirm }),
    })
  })
  if (!confirm) return
  saving.value = true
  const res = await handleAlarm(id.value, { status: 'resolved' })
  saving.value = false
  if (res.error) return toastError(res.error)
  uni.$emit('alarm:changed')
  load()
}

function openNote() {
  noteText.value = ''
  noteVisible.value = true
}

async function saveNote() {
  if (!noteText.value.trim()) {
    uni.showToast({ title: '请输入备注内容', icon: 'none' })
    return
  }
  saving.value = true
  const res = await handleAlarm(id.value, { note: noteText.value.trim() })
  saving.value = false
  if (res.error) return toastError(res.error)
  noteVisible.value = false
  uni.$emit('alarm:changed')
  uni.showToast({ title: '备注已记录', icon: 'success' })
  load()
}

function goRemote() {
  if (!isAdmin()) return
  gotoTab('control', { space_id: alarm.value ? alarm.value.space_id : '' })
}
function goBack() {
  uni.navigateBack({ fail: () => uni.redirectTo({ url: '/pages/shell/shell' }) })
}
</script>

<style lang="scss" scoped>
.page { padding-bottom: 200rpx; }

.head {
  margin: 20rpx $gap;
  background: $card;
  border-radius: $radius;
  padding: 32rpx;
  box-shadow: $shadow;
  border-top: 8rpx solid $info;
}
.head--critical { border-top-color: $danger; }
.head--warning { border-top-color: $warning; }
.head--info { border-top-color: $primary; }
.head__row { display: flex; align-items: center; }
.head__icon {
  width: 88rpx;
  height: 88rpx;
  border-radius: $r-icon;
  @include flex(center, center);
  margin-right: $sp-24;
  flex: none;
  background: $danger-light;
}
.head__icon--warning { background: $warning-light; }
.head__icon--info { background: $primary-light; }
.head__main { flex: 1; }
.head__title { font-size: $fs-xl; font-weight: 700; }
.head__tags { display: flex; gap: 12rpx; margin-top: 12rpx; }
.head__summary { font-size: $fs-md; color: $text-2; margin-top: 20rpx; }
.head__meta { display: flex; gap: 30rpx; margin-top: 16rpx; font-size: $fs-aux; color: $text-3; }
.head__meta-item { display: flex; align-items: center; gap: 8rpx; }
.head__note {
  margin-top: 20rpx;
  background: $disabled-light;
  border-radius: $radius-sm;
  padding: 16rpx 20rpx;
  font-size: $fs-xs;
  color: $text-2;
  line-height: 36rpx;
}

.tl { padding-left: 8rpx; }
.tl__item { display: flex; }
.tl__rail { display: flex; flex-direction: column; align-items: center; width: 40rpx; }
.tl__dot {
  width: 20rpx; height: 20rpx; border-radius: 50%;
  background: $primary; margin-top: 10rpx;
  box-shadow: 0 0 0 8rpx $primary-light;
}
.tl__dot--0 { background: $danger; box-shadow: 0 0 0 8rpx $danger-light; }
.tl__line { flex: 1; width: 3rpx; background: $border; margin: 6rpx 0; }
.tl__body { flex: 1; padding: 0 0 36rpx 20rpx; }
.tl__row { display: flex; align-items: center; justify-content: space-between; }
.tl__stage { font-size: $fs-md; font-weight: 600; }
.tl__time { font-size: $fs-xs; color: $text-3; }
.tl__note { display: block; font-size: $fs-sm; color: $text-2; margin-top: 8rpx; line-height: 38rpx; }
.tl__by { display: block; font-size: $fs-xs; color: $text-3; margin-top: 6rpx; }

.action-bar {
  position: fixed;
  left: 0; right: 0; bottom: 0;
  display: flex;
  gap: 16rpx;
  padding: 20rpx $gap calc(20rpx + constant(safe-area-inset-bottom));
  padding-bottom: calc(20rpx + env(safe-area-inset-bottom));
  background: $bg-card;
  box-shadow: $shadow-bar;
  z-index: 997;
}
.action-bar__main { flex: 1; }

.mask {
  position: fixed; inset: 0;
  background: $mask;
  z-index: 999;
  display: flex;
  align-items: flex-end;
}
.popup {
  width: 100%;
  background: $bg-card;
  border-radius: $radius $radius 0 0;
  padding: 36rpx 36rpx calc(36rpx + env(safe-area-inset-bottom));
}
.popup__title { font-size: $fs-lg; font-weight: 600; }
.popup__textarea {
  width: 100%;
  box-sizing: border-box;
  height: 200rpx;
  background: $disabled-light;
  border-radius: $radius-sm;
  padding: 20rpx;
  font-size: $fs-md;
  margin-top: 24rpx;
}
.popup__btns { display: flex; gap: 20rpx; margin-top: 28rpx; }
.popup__btns .c-btn { flex: 1; }
</style>
