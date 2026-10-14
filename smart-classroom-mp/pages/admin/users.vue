<template>
  <view class="page">
    <offline-banner :ws="false" />

    <state-view
      :loading="loading"
      :error="loadError"
      :perm="permOk === false ? false : null"
      perm-text="用户管理为管理员功能"
      empty-text="暂无用户"
      empty-desc="用户由云端同步，暂无数据"
      empty-action="重新加载"
      empty-icon="user"
      @retry="load(true)"
      @perm-action="goHome"
      @empty-action="load(true)"
    >
      <view v-for="u in users" :key="u.user_id" class="card user-card">
        <view class="user-card__head" @tap="toggleExpand(u.user_id)">
          <text class="user-card__avatar" :class="u.role === 'admin' ? 'user-card__avatar--admin' : ''">
            {{ (u.name || '?').slice(0, 1) }}
          </text>
          <view class="user-card__main">
            <view class="user-card__name-row">
              <text class="user-card__name">{{ u.name }}</text>
              <view class="tag" :class="u.role === 'admin' ? 'tag--blue' : 'tag--gray'">
                {{ u.role === 'admin' ? '管理员' : '教师' }}
              </view>
            </view>
            <text class="user-card__sub">@{{ u.username }} · 绑定 {{ (u.spaces || []).length }} 间教室</text>
          </view>
          <text class="user-card__arrow" :class="{ 'user-card__arrow--open': expanded === u.user_id }">
            <app-icon name="chevron-right" :size="28" :color="COLORS.text3" />
          </text>
        </view>

        <!-- 展开详情：绑定管理 -->
        <view v-if="expanded === u.user_id" class="bind-zone">
          <view class="bind-zone__title">教室绑定关系</view>
          <view class="bind-zone__chips">
            <view v-for="s in u.space_names" :key="s" class="bchip">
              <text>{{ s }}</text>
              <app-icon name="close" :size="20" :color="COLORS.primary" class="bchip__x" @tap="unbind(u, s)" />
            </view>
            <view v-if="!(u.spaces || []).length" class="bind-zone__empty">尚未绑定任何教室</view>
          </view>

          <!-- 绑定新教室 -->
          <view class="bind-zone__add">
            <picker
              :range="bindableNames(u)"
              :disabled="!bindableNames(u).length"
              @change="e => bind(u, e)"
            >
              <view class="c-btn c-btn--ghost c-btn--sm add-btn">＋ 绑定新教室</view>
            </picker>
          </view>
        </view>
      </view>
    </state-view>

    <view class="safe-bottom"></view>
  </view>
</template>

<script setup>
/**
 * P9 用户与教室绑定管理（管理员）
 * - 用户列表 → 展开详情；可绑定/解绑多教室关系
 * - 解绑操作二次确认
 */
import { ref } from 'vue'
import { onShow, onPullDownRefresh } from '@dcloudio/uni-app'
import { getUsers, bindUserSpace, unbindUserSpace } from '../../api'
import { toastError } from '../../api/request'
import { can, requireAuth, gotoTab } from '../../utils/permission'
import { store } from '../../store'
import { COLORS } from '../../config/theme'

const loading = ref(true)
const loadError = ref('')
const permOk = ref(null)
const users = ref([])
const expanded = ref('')

async function load(showLoading = false) {
  if (!requireAuth()) return
  permOk.value = can('user:manage')
  if (!permOk.value) { loading.value = false; return }
  if (showLoading) loading.value = true
  loadError.value = ''
  const res = await getUsers()
  if (res.error) loadError.value = res.error.message
  else users.value = res.data || []
  loading.value = false
  uni.stopPullDownRefresh()
}

onShow(() => {
  if (!requireAuth()) return
  load(true)
})
onPullDownRefresh(() => load())

function toggleExpand(id) {
  expanded.value = expanded.value === id ? '' : id
}

function bindableNames(u) {
  return store.spaces
    .filter((s) => !(u.spaces || []).includes(s.space_id))
    .map((s) => s.name)
}

async function bind(u, e) {
  const idx = Number(e.detail.value)
  const names = bindableNames(u)
  if (Number.isNaN(idx) || !names[idx]) return
  const space = store.spaces.find((s) => s.name === names[idx])
  if (!space) return
  const res = await bindUserSpace(u.user_id, space.space_id)
  if (res.error) return toastError(res.error)
  uni.showToast({ title: `已绑定「${space.name}」`, icon: 'success' })
  load()
}

async function unbind(u, spaceName) {
  const space = store.spaces.find((s) => s.name === spaceName)
  if (!space) return
  const { confirm } = await new Promise((resolve) => {
    uni.showModal({
      title: '解绑教室',
      content: `确认解除「${u.name}」与「${spaceName}」的绑定？解绑后该教师将无法查看此教室。`,
      confirmColor: COLORS.danger,
      confirmText: '确认解绑',
      success: (r) => resolve({ confirm: !!r.confirm }),
    })
  })
  if (!confirm) return
  const res = await unbindUserSpace(u.user_id, space.space_id)
  if (res.error) return toastError(res.error)
  uni.showToast({ title: '已解绑', icon: 'success' })
  load()
}

function goHome() {
  gotoTab('overview')
}
</script>

<style lang="scss" scoped>
.page { padding-bottom: 40rpx; }

.user-card__head { display: flex; align-items: center; }
.user-card__avatar {
  width: 84rpx; height: 84rpx; border-radius: 50%;
  background: $success-light; color: $success;
  display: flex; align-items: center; justify-content: center;
  font-size: $fs-lg; font-weight: 700; margin-right: 20rpx; flex-shrink: 0;
}
.user-card__avatar--admin { background: $primary-light; color: $primary; }
.user-card__main { flex: 1; min-width: 0; }
.user-card__name-row { display: flex; align-items: center; gap: 12rpx; }
.user-card__name { font-size: $fs-lg; font-weight: 600; }
.user-card__sub { display: block; font-size: $fs-xs; color: $text-3; margin-top: 6rpx; }
.user-card__arrow {
  font-size: 44rpx; color: $text-3;
  transform: rotate(90deg); transition: transform 0.2s;
}
.user-card__arrow--open { transform: rotate(-90deg); }

.bind-zone { margin-top: 24rpx; border-top: 1rpx solid $border; padding-top: 24rpx; }
.bind-zone__title { font-size: $fs-sm; color: $text-2; font-weight: 600; margin-bottom: 16rpx; }
.bind-zone__chips { display: flex; flex-wrap: wrap; gap: 14rpx; }
.bind-zone__empty { font-size: $fs-xs; color: $text-3; }
.bchip {
  display: inline-flex;
  align-items: center;
  background: $primary-light;
  color: $primary;
  border-radius: $r-tag;
  padding: 10rpx 24rpx;
  font-size: $fs-sm;
}
.bchip__x { margin-left: 12rpx; opacity: 0.7; padding: 0 4rpx; }
.bind-zone__add { margin-top: 24rpx; }
.add-btn { width: 300rpx; }
</style>
