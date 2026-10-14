<template>
  <view class="pf">
    <!-- 用户信息头 -->
    <view class="pf__head">
      <view class="pf__avatar">{{ (userName || '?').slice(0, 1) }}</view>
      <view class="pf__main">
        <view class="pf__name-row">
          <text class="pf__name">{{ userName }}</text>
          <view class="pf__role-tag">{{ isAdmin() ? '管理员' : '教师' }}</view>
        </view>
        <text class="pf__sub">@{{ (store.user && store.user.username) || '-' }}</text>
      </view>
    </view>

    <view class="card">
      <view class="cell">
        <text class="cell__label">当前角色</text>
        <text class="cell__value">{{ isAdmin() ? '管理员（全局权限）' : '教师（本教室权限）' }}</text>
      </view>
      <view class="cell">
        <text class="cell__label">绑定教室</text>
        <text class="cell__value">{{ boundText }}</text>
      </view>
    </view>

    <!-- 订阅管理：额度可耗尽，随时补充授权 -->
    <view class="card">
      <view class="cell" hover-class="press" @tap="requestSub">
        <view class="pf__cell-left">
          <app-icon name="bell" :size="32" :color="COLORS.text2" />
          <text class="cell__label">告警推送订阅</text>
        </view>
        <view class="pf__cell-right">
          <text class="cell__value">{{ store.subscribed ? '已授权（额度随消耗递减）' : '未开启' }}</text>
          <app-icon name="back" :size="28" :color="COLORS.text4" class="pf__arrow" />
        </view>
      </view>
      <view class="pf__subnote">
        微信订阅消息为「一次授权、一条推送」的额度机制。点击上方入口可随时补充授权额度，授权被拒后不会反复打扰。
      </view>
    </view>

    <view class="card" v-if="isAdmin()">
      <view class="cell" hover-class="press" @tap="goUsers">
        <view class="pf__cell-left">
          <app-icon name="user" :size="32" :color="COLORS.text2" />
          <text class="cell__label">用户与教室绑定</text>
        </view>
        <app-icon name="back" :size="28" :color="COLORS.text4" class="pf__arrow" />
      </view>
      <view class="cell" hover-class="press" @tap="goHistory">
        <view class="pf__cell-left">
          <app-icon name="clock" :size="32" :color="COLORS.text2" />
          <text class="cell__label">告警历史查询</text>
        </view>
        <app-icon name="back" :size="28" :color="COLORS.text4" class="pf__arrow" />
      </view>
    </view>

    <!-- 教师无「我的」tab：提供返回教室入口 -->
    <view class="card" v-else>
      <view class="cell" hover-class="press" @tap="goClassroom">
        <view class="pf__cell-left">
          <app-icon name="home" :size="32" :color="COLORS.text2" />
          <text class="cell__label">返回我的教室</text>
        </view>
        <app-icon name="back" :size="28" :color="COLORS.text4" class="pf__arrow" />
      </view>
    </view>

    <!-- 退出登录（危险操作：红色 + 底部 + 确认文案，三重区分） -->
    <view class="pf__logout">
      <button class="c-btn c-btn--danger" hover-class="press" @tap="logout">退出登录</button>
    </view>

    <view class="safe-bottom" />
  </view>
</template>

<script setup>
/**
 * P10 我的 · 壳层视图（管理员 tab / 教师从教室首页进入）
 * 角色/姓名/绑定教室 + 订阅管理 + 退出登录（确认）
 */
import { computed } from 'vue'
import { store, clearAuth, setSubscribed } from '../../store'
import { closeWs } from '../../services/ws'
import { isAdmin, requireAuth, gotoTab } from '../../utils/permission'
import { SUBSCRIBE_TMPL_IDS } from '../../api/config'
import { COLORS } from '../../config/theme'

const userName = computed(() => (store.user && store.user.name) || '')
const boundText = computed(() => {
  if (isAdmin()) return '全部教室（管理员）'
  const s = store.spaces.find((x) => x.space_id === store.spaceId)
  return (s && s.name) || '未绑定'
})

function onEnter() { requireAuth() }
function onRefresh() {}

function requestSub() {
  // #ifdef MP-WEIXIN
  if (!SUBSCRIBE_TMPL_IDS[0] || SUBSCRIBE_TMPL_IDS[0] === 'TEMPLATE_ID_PLACEHOLDER') {
    uni.showToast({ title: '请先在 api/config.js 配置订阅模板 ID', icon: 'none' })
    return
  }
  uni.requestSubscribeMessage({
    tmplIds: SUBSCRIBE_TMPL_IDS,
    success: (res) => {
      if (res[SUBSCRIBE_TMPL_IDS[0]] === 'accept') {
        setSubscribed(true)
        uni.showToast({ title: '已补充一条推送额度', icon: 'none' })
      } else {
        uni.showToast({ title: '已拒绝授权', icon: 'none' })
      }
    },
    fail: () => uni.showToast({ title: '授权未完成', icon: 'none' }),
  })
  // #endif
  // #ifndef MP-WEIXIN
  uni.showToast({ title: '请在微信小程序中完成订阅授权', icon: 'none' })
  // #endif
}

function logout() {
  uni.showModal({
    title: '退出登录',
    content: '退出后将无法接收告警推送，确认退出？',
    confirmText: '退出',
    confirmColor: COLORS.danger,
    success: (res) => {
      if (!res.confirm) return
      closeWs()
      clearAuth()
      uni.reLaunch({ url: '/pages/login/login' })
    },
  })
}

function goUsers() { uni.navigateTo({ url: '/pages/admin/users' }) }
function goHistory() { uni.navigateTo({ url: '/pages/admin/history' }) }
function goClassroom() { gotoTab('classroom') }

defineExpose({ onEnter, onRefresh })
</script>

<style lang="scss" scoped>
.pf { padding-bottom: $sp-40; }

.pf__head {
  margin: $sp-20 $page-pad;
  background: linear-gradient(135deg, #F8FAFF 0%, #EBF3FF 100%);
  border-radius: $r-card;
  padding: 40rpx $sp-32;
  color: $text-1;
  @include flex(flex-start, center);
  box-shadow: 0 8rpx 28rpx rgba(0, 122, 255, 0.08);
  border: 1rpx solid rgba(0, 122, 255, 0.12);
}
.pf__avatar {
  width: 110rpx;
  height: 110rpx;
  border-radius: 50%;
  background: linear-gradient(135deg, #4A9EFF 0%, #007AFF 100%);
  color: #fff;
  @include flex(center, center);
  font-size: 48rpx;
  font-weight: 700;
  margin-right: 26rpx;
  box-shadow: 0 8rpx 22rpx rgba(0, 122, 255, 0.30);
}
.pf__main { flex: 1; min-width: 0; }
.pf__name-row { @include flex(flex-start, center); gap: 14rpx; }
.pf__name { font-size: $fs-page-title; font-weight: 700; color: $text-1; }
.pf__role-tag {
  font-size: $fs-tag;
  line-height: 34rpx;
  padding: 0 14rpx;
  border-radius: $r-tag;
  background: $primary-light;
  color: $primary;
}
.pf__sub { display: block; color: $text-2; font-size: $fs-aux; margin-top: 8rpx; }

.pf__cell-left { @include flex(flex-start, center); gap: $sp-12; flex: 1; min-width: 0; }
.pf__cell-left .cell__label { flex: none; }
.pf__cell-right { @include flex(flex-end, center); gap: 4rpx; }

.pf__arrow { transform: rotate(180deg); }

.pf__subnote {
  font-size: $fs-tag;
  color: $text-3;
  line-height: 38rpx;
  padding: $sp-16 0 0;
  border-top: 1rpx dashed $line;
  margin-top: 8rpx;
}

.pf__logout { margin: 40rpx $page-pad 0; }
</style>
