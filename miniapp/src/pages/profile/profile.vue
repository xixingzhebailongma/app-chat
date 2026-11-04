<template>
  <view class="profile">
    <!-- 用户信息 Hero 头 -->
    <view class="pf__head">
      <view class="pf__avatar">{{ avatarChar }}</view>
      <view class="pf__main">
        <view class="pf__name-row">
          <text class="pf__name">{{ store.name || '未登录' }}</text>
          <view class="pf__role-tag">{{ roleLabel }}</view>
        </view>
        <text class="pf__sub">@{{ store.userId || '-' }}</text>
      </view>
    </view>

    <!-- 角色与绑定信息 -->
    <view class="card info-card">
      <view class="cell">
        <text class="cell__label">当前角色</text>
        <text class="cell__value">{{ roleScopeLabel }}</text>
      </view>
    </view>

    <!-- 消息订阅 -->
    <view class="card info-card">
      <view class="cell">
        <view class="cell__left">
          <text class="cell__label">告警推送订阅</text>
        </view>
        <view class="cell__right">
          <text class="cell__value">{{ subscribed ? '已开启' : '未开启' }}</text>
          <switch :checked="subscribed" color="var(--primary)" @change="onSubscribe" style="transform: scale(0.7); transform-origin: right center;" />
        </view>
      </view>
      <view class="pf__subnote">微信订阅消息为「一次授权、一条推送」的额度机制，点击开关可随时补充授权额度。真实网关模式下订阅模板需在网关侧配置（SUBSCRIBE_TMPL_IDS）；演练模式下该动作为模拟。</view>
    </view>

    <!-- 关于 -->
    <view class="card info-card">
      <view class="cell">
        <text class="cell__label">当前版本</text>
        <text class="cell__value">{{ version }} · {{ modeLabel }}</text>
      </view>
    </view>

    <!-- 会话说明（1-4 改写：保留真实退出登录，另给一个只读说明入口） -->
    <view class="card info-card">
      <view class="cell cell--tap" hover-class="cell--hover" :hover-stay-time="60" @tap="openSessionNote">
        <text class="cell__label">会话说明</text>
        <text class="cell__value">{{ modeLabel }} · 查看</text>
      </view>
    </view>

    <!-- 退出登录 -->
    <view class="pf__logout">
      <button class="btn btn-danger" @tap="doLogout">退出登录</button>
    </view>

    <!-- 会话说明弹层（独立 class，与告警处置弹层互不依赖） -->
    <view v-if="noteVisible" class="sn__mask" @tap="closeSessionNote">
      <view class="sn__sheet" @tap.stop>
        <text class="sn__title">{{ sessionTitle }}</text>
        <text class="sn__id">{{ sessionIdentity }}</text>
        <text class="sn__text">{{ sessionBody }}</text>
        <text class="sn__foot">本说明仅说明当前会话性质，不涉及任何账号操作。</text>
        <button class="sn__btn" @tap="closeSessionNote">我知道了</button>
      </view>
    </view>

    <AppTabBar current="profile" />
  </view>
</template>

<script>
import config from '../../config'
import { api } from '../../api/index'
import { requestSubscribe } from '../../utils/subscribe'
import { store, isLoggedIn, logout } from '../../store/index'
import AppTabBar from '../../components/AppTabBar.vue'

export default {
  components: { AppTabBar },
  data() {
    return {
      store,
      subscribed: true,
      version: config.APP_VERSION,
      noteVisible: false,
    }
  },
  computed: {
    roleLabel() {
      const base = store.role === 'admin' ? '管理员' : '教师'
      return config.USE_MOCK ? base + ' · 演示' : base
    },
    roleScopeLabel() {
      const base = store.role === 'admin' ? '管理员（全局权限）' : '教师（本教室权限）'
      return config.USE_MOCK ? base + ' · 演示' : base
    },
    sessionTitle() {
      return config.USE_MOCK ? '演练会话说明' : '会话说明'
    },
    sessionIdentity() {
      return (store.role === 'admin' ? '管理员' : '教师') + ' · ' + (config.USE_MOCK ? '演练' : '已连接网关')
    },
    sessionBody() {
      return config.USE_MOCK
        ? '当前处于前端独立演练环境，无真实云端账号会话。所有设备控制、空间拓扑与告警流转均为本地内存模拟闭环。'
        : '当前已连接本地网关（' + config.BASE_URL + '），设备控制与告警处置会真实下发。本说明仅说明当前会话性质。'
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
    openSessionNote() {
      this.noteVisible = true
    },
    closeSessionNote() {
      this.noteVisible = false
    },
    onSubscribe(e) {
      const enabled = e.detail.value
      if (!enabled) {
        if (config.USE_MOCK) {
          this.subscribed = false
          return
        }
        api.subscribe({ unsubscribe_all: true })
          .then(() => this._refreshSubscription())
          .catch((err) => uni.showToast({ title: err.message || '操作失败', icon: 'none' }))
        return
      }
      if (config.USE_MOCK) {
        this.subscribed = true
        return
      }
      requestSubscribe().then(() => this._refreshSubscription())
    },
    async _refreshSubscription() {
      try {
        const r = await api.getSubscriptions()
        const templates = (r && r.templates) || {}
        this.subscribed = Object.values(templates).some((t) => t && t.active)
      } catch (err) {}
    },
    doLogout() {
      uni.showModal({
        title: '退出登录',
        content: '退出后将无法接收告警推送，确认退出？',
        confirmText: '退出',
        confirmColor: '#C62828',
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
  padding-top: 20rpx;
  /* 底部让位与全局 .page-pad 对齐：固定 TabBar（100rpx+安全区）+ 呼吸空间 */
  padding-bottom: calc(240rpx + env(safe-area-inset-bottom));
}

/* Hero 头部 */
.pf__head {
  margin: 20rpx 36rpx 24rpx;
  background: linear-gradient(135deg, var(--bg-card) 0%, var(--brand-soft) 100%);
  border-radius: 24rpx;
  padding: 40rpx 32rpx;
  display: flex;
  align-items: center;
  box-shadow: 0 8rpx 28rpx rgba(36, 94, 112, 0.08);
  border: 1rpx solid rgba(36, 94, 112, 0.12);
}
.pf__avatar {
  width: 110rpx;
  height: 110rpx;
  border-radius: 50%;
  background: var(--brand-grad);
  color: #fff;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 40rpx;
  font-weight: 600;
  margin-right: 26rpx;
  box-shadow: 0 8rpx 22rpx rgba(36, 94, 112, 0.30);
  flex-shrink: 0;
}
.pf__main {
  flex: 1;
  min-width: 0;
}
.pf__name-row {
  display: flex;
  align-items: center;
  gap: 14rpx;
}
.pf__name {
  font-size: 36rpx;
  font-weight: 600;
  color: var(--text-1);
}
.pf__role-tag {
  font-size: 24rpx;
  font-weight: 400;
  line-height: 34rpx;
  padding: 0 14rpx;
  border-radius: 8rpx;
  background: var(--primary-soft);
  color: var(--primary);
}
.pf__sub {
  display: block;
  color: var(--text-3);
  font-size: 26rpx;
  margin-top: 8rpx;
}

/* 信息卡片 */
.info-card {
  margin: 0 36rpx 28rpx;
  padding: 0 32rpx;
}
.cell {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 28rpx 0;
  border-bottom: 1rpx solid var(--border-lighter);
}
.cell:last-child {
  border-bottom: none;
}
.cell__label {
  font-size: 28rpx;
  font-weight: 400;
  color: var(--text-1);
}
.cell__value {
  font-size: 28rpx;
  font-weight: 400;
  color: var(--text-3);
}
.cell__left {
  flex: 1;
}
.cell__right {
  display: flex;
  align-items: center;
  gap: 12rpx;
}
.pf__subnote {
  font-size: 24rpx;
  color: var(--text-3);
  line-height: 38rpx;
  padding: 16rpx 0 4rpx;
  border-top: 1rpx dashed var(--border-lighter);
  margin-top: 4rpx;
}

/* 退出登录 */
.pf__logout {
  margin: 40rpx 36rpx 0;
}
.btn-danger {
  background: var(--bg-card);
  color: var(--danger);
  border: 1rpx solid var(--danger-border);
  border-radius: 999rpx;
  font-size: 28rpx;
  font-weight: 500;
}
.btn-danger::after {
  border: none;
}

/* 会话说明入口 */
.cell--tap {
  transition: opacity 150ms ease;
}
.cell--hover {
  opacity: 0.6;
}

/* 会话说明弹层（独立命名，避免与告警处置弹层的样式互相影响） */
.sn__mask {
  position: fixed;
  left: 0;
  right: 0;
  top: 0;
  bottom: 0;
  background: var(--bg-mask);
  display: flex;
  align-items: flex-end;
  z-index: 999;
}
.sn__sheet {
  width: 100%;
  box-sizing: border-box;
  padding: 40rpx 40rpx calc(40rpx + env(safe-area-inset-bottom));
  background: linear-gradient(140deg, var(--bg-card), var(--bg-card-end));
  border-radius: 32rpx 32rpx 0 0;
  box-shadow: 0 -12rpx 48rpx rgba(96, 81, 51, 0.12);
}
.sn__title {
  display: block;
  font-size: 36rpx;
  font-weight: 600;
  color: var(--text-1);
}
.sn__id {
  display: inline-block;
  margin-top: 20rpx;
  font-size: 24rpx;
  line-height: 34rpx;
  padding: 0 14rpx;
  border-radius: 8rpx;
  background: var(--primary-soft);
  color: var(--primary);
}
.sn__text {
  display: block;
  margin-top: 20rpx;
  font-size: 28rpx;
  line-height: 46rpx;
  color: var(--text-2);
}
.sn__foot {
  display: block;
  margin-top: 20rpx;
  font-size: 24rpx;
  line-height: 38rpx;
  color: var(--text-3);
}
.sn__btn {
  margin-top: 36rpx;
  width: 100%;
  min-height: 88rpx;
  line-height: 88rpx;
  background: var(--primary);
  color: #ffffff;
  font-size: 30rpx;
  font-weight: 500;
  border-radius: 999rpx;
  border: none;
}
.sn__btn::after {
  border: none;
}
</style>
