<template>
  <view class="page">
    <!-- 用户信息头 -->
    <view class="profile">
      <view class="profile__avatar">{{ (userName || '?').slice(0, 1) }}</view>
      <view class="profile__main">
        <view class="profile__name-row">
          <text class="profile__name">{{ userName }}</text>
          <view class="tag tag--white">{{ isAdmin() ? '管理员' : '教师' }}</view>
        </view>
        <text class="profile__sub">@{{ (store.user && store.user.username) || '-' }}</text>
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
      <view class="cell" @tap="requestSub">
        <text class="cell__label">🔔 告警推送订阅</text>
        <view class="cell__right">
          <text class="cell__value">{{ store.subscribed ? '已授权（额度随消耗递减）' : '未开启' }}</text>
          <text class="cell__arrow">›</text>
        </view>
      </view>
      <view class="sub-note">
        微信订阅消息为「一次授权、一条推送」的额度机制。点击上方入口可随时补充授权额度，授权被拒后不会反复打扰。
      </view>
    </view>

    <view class="card">
      <view class="cell" @tap="goUsers" v-if="isAdmin()">
        <text class="cell__label">👥 用户与教室绑定</text>
        <text class="cell__arrow">›</text>
      </view>
      <view class="cell" @tap="goHistory" v-if="isAdmin()">
        <text class="cell__label">📅 告警历史查询</text>
        <text class="cell__arrow">›</text>
      </view>
    </view>

    <!-- 退出登录（危险操作：红色 + 底部 + 确认文案，三重区分） -->
    <view class="logout-zone">
      <button class="c-btn c-btn--danger" @tap="logout">退出登录</button>
    </view>

    <view v-if="isAdmin()" class="safe-bottom"></view>
    <TabBar v-if="isAdmin()" current="/pages/profile/profile" />
  </view>
</template>

<script setup>
/**
 * P10 我的：角色/姓名/绑定教室 + 订阅管理 + 退出登录（确认）
 */
import { computed } from 'vue'
import { onShow } from '@dcloudio/uni-app'
import { store, clearAuth, setSubscribed } from '../../store'
import { closeWs } from '../../services/ws'
import { isAdmin, requireAuth } from '../../utils/permission'
import { SUBSCRIBE_TMPL_IDS } from '../../api/config'
import TabBar from '../../components/TabBar.vue'

const userName = computed(() => (store.user && store.user.name) || '')
const boundText = computed(() => {
  if (isAdmin()) return '全部教室（管理员）'
  const s = store.spaces.find((x) => x.space_id === store.spaceId)
  return (s && s.name) || '未绑定'
})

onShow(() => { requireAuth() })

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
    confirmColor: '#DC2626',
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
</script>

<style lang="scss" scoped>
.page { padding-bottom: 60rpx; }

.profile {
  margin: 20rpx $gap;
  background: linear-gradient(135deg, #334155, #1E293B);
  border-radius: $radius;
  padding: 40rpx 32rpx;
  color: #fff;
  display: flex;
  align-items: center;
  box-shadow: 0 8rpx 24rpx rgba(30, 41, 59, 0.25);
}
.profile__avatar {
  width: 110rpx; height: 110rpx; border-radius: 50%;
  background: rgba(255, 255, 255, 0.15);
  display: flex; align-items: center; justify-content: center;
  font-size: 48rpx; font-weight: 700;
  margin-right: 26rpx; border: 3rpx solid rgba(255, 255, 255, 0.3);
}
.profile__name-row { display: flex; align-items: center; gap: 14rpx; }
.profile__name { font-size: $fs-xl; font-weight: 700; }
.profile__sub { display: block; opacity: 0.7; font-size: $fs-sm; margin-top: 8rpx; }

.tag--white { background: rgba(255, 255, 255, 0.18); color: #fff; }

.cell__right { display: flex; align-items: center; }
.cell__arrow { color: $text-3; font-size: 40rpx; margin-left: 8rpx; }

.sub-note {
  font-size: $fs-xs;
  color: $text-3;
  line-height: 38rpx;
  padding: 16rpx 0 0;
  border-top: 1rpx dashed $border;
  margin-top: 8rpx;
}

.logout-zone { margin: 40rpx $gap 0; }
</style>
