<template>
  <view class="profile page-pad">
    <!-- 用户信息 -->
    <view class="user card">
      <view class="avatar">{{ avatarChar }}</view>
      <view class="u-main">
        <text class="u-name">{{ store.name || '未登录' }}</text>
        <view class="u-meta">
          <StatusTag :text="roleLabel" :tone="store.role === 'admin' ? 'info' : 'neutral'" />
          <text class="u-id">{{ store.userId }}</text>
        </view>
      </view>
    </view>

    <!-- 订阅消息 -->
    <view class="section-title"><text class="t">消息订阅</text></view>
    <view class="list-group">
      <view class="list-cell list-cell-center">
        <view class="cell-main">
          <text class="cell-title">订阅消息通知</text>
          <text class="cell-desc">开启后，设备离线、告警等将通过微信订阅消息推送</text>
        </view>
        <switch :checked="subscribed" color="#409eff" @change="onSubscribe" />
      </view>
    </view>

    <!-- 关于 -->
    <view class="section-title"><text class="t">关于</text></view>
    <view class="list-group">
      <view class="list-cell list-cell-center">
        <view class="cell-main">
          <text class="cell-title">当前版本</text>
          <text class="cell-desc">{{ version }}</text>
        </view>
        <text class="cell-val">{{ modeLabel }}</text>
      </view>
    </view>

    <button class="btn btn-plain logout" @tap="doLogout">退出登录</button>

    <AppTabBar current="profile" />
  </view>
</template>

<script>
import config from '../../config'
import { api } from '../../api/index'
import { requestSubscribe } from '../../utils/subscribe'
import { store, isLoggedIn, logout } from '../../store/index'
import StatusTag from '../../components/StatusTag.vue'
import AppTabBar from '../../components/AppTabBar.vue'

export default {
  components: { StatusTag, AppTabBar },
  data() {
    return {
      store,
      subscribed: true,
      version: '1.0.0',
    }
  },
  computed: {
    roleLabel() {
      return store.role === 'admin' ? '管理员' : '教师'
    },
    avatarChar() {
      return (store.name || '师').slice(0, 1)
    },
    modeLabel() {
      return config.USE_MOCK ? '演示模式' : '已连接网关'
    },
  },
  onShow() {
    if (!isLoggedIn()) {
      uni.reLaunch({ url: '/pages/login/login' })
      return
    }
    if (!config.USE_MOCK) {
      this._refreshSubscription()
    }
  },
  methods: {
    onSubscribe(e) {
      const enabled = e.detail.value
      if (!enabled) {
        // 关闭：清空全部订阅（真实模式）或本地切换（演示模式）。
        if (config.USE_MOCK) {
          this.subscribed = false
          return
        }
        api.subscribe({ unsubscribe_all: true })
          .then(() => this._refreshSubscription())
          .catch((err) => uni.showToast({ title: err.message || '操作失败', icon: 'none' }))
        return
      }

      // 开启。
      if (config.USE_MOCK) {
        this.subscribed = true
        return
      }
      // 微信要求 requestSubscribeMessage 必须在用户点击的同步调用栈内调用；
      // 复用 utils/subscribe.js 的共享实现（含未配置模板 ID / ban / 失败提示）。
      requestSubscribe().then(() => this._refreshSubscription())
    },

    async _refreshSubscription() {
      try {
        const r = await api.getSubscriptions()
        const templates = (r && r.templates) || {}
        this.subscribed = Object.values(templates).some((t) => t && t.active)
      } catch (err) {
        // 回显失败不打断页面，保留当前开关状态。
      }
    },
    doLogout() {
      uni.showModal({
        title: '退出登录',
        content: '确定要退出当前账号吗？',
        success: (res) => {
          if (res.confirm) {
            logout()
            uni.reLaunch({ url: '/pages/login/login' })
          }
        },
      })
    },
  },
}
</script>

<style scoped>
.profile {
  /* 注意：bottom 交给 .page-pad 让位给 tabbar，这里不要用 padding shorthand */
  padding-top: 24rpx;
  padding-left: 24rpx;
  padding-right: 24rpx;
}
.user {
  display: flex;
  align-items: center;
  padding: 40rpx 32rpx;
}
.avatar {
  width: 120rpx;
  height: 120rpx;
  border-radius: 12rpx;
  margin-right: 28rpx;
  display: flex;
  align-items: center;
  justify-content: center;
  background: var(--primary);
  color: #fff;
  font-size: 52rpx;
  font-weight: 700;
}
.u-main {
  flex: 1;
  display: flex;
  flex-direction: column;
}
.u-name {
  font-size: 36rpx;
  font-weight: 700;
  color: var(--text-1);
}
.u-meta {
  display: flex;
  align-items: center;
  margin-top: 14rpx;
}
.u-id {
  margin-left: 16rpx;
  font-size: 22rpx;
  color: var(--text-4);
}

.cell-main {
  flex: 1;
  display: flex;
  flex-direction: column;
}
.cell-title {
  font-size: 30rpx;
  color: var(--text-1);
}
.cell-desc {
  margin-top: 6rpx;
  font-size: 22rpx;
  color: var(--text-4);
}
.cell-val {
  font-size: 24rpx;
  color: var(--text-3);
}

.logout {
  margin-top: 56rpx;
  color: var(--danger);
}
</style>
