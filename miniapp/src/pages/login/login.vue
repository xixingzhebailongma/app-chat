<template>
  <view class="login">
    <view class="hero">
      <view class="logo"><uni-icons type="home" size="48" color="#ffffff" /></view>
      <text class="name">智慧校园数字基座</text>
      <text class="slogan">设备状态 · 告警处理 · 空间管理</text>
    </view>

    <view class="panel card">
      <text class="tip">{{ tip }}</text>
      <input v-model="username" class="field" placeholder="账号" placeholder-class="ph" />
      <input v-model="password" class="field" password placeholder="密码" placeholder-class="ph" />
      <button class="btn btn-primary submit" :loading="loading" @tap="submit">{{ submitText }}</button>
    </view>

    <text class="footer">© 智慧校园数字基座</text>
  </view>
</template>

<script>
import config from '../../config'
import { api } from '../../api/index'
import { store, applySession, isLoggedIn } from '../../store/index'
import { refreshTemplateIds } from '../../utils/subscribe'

export default {
  data() {
    return {
      isMock: config.USE_MOCK,
      // mock 模式不预填；演示账号：admin/admin123（管理员）、li/123456、wang/123456（教师）。
      username: '',
      password: '',
      loading: false,
    }
  },
  computed: {
    tip() {
      return '请输入账号密码登录'
    },
    submitText() {
      return this.isMock ? '登录' : '登录 / 绑定'
    },
  },
  onLoad() {
    if (isLoggedIn()) {
      uni.reLaunch({ url: store.role === 'teacher' ? '/pages/classroom/classroom' : '/pages/home/home' })
    }
  },
  methods: {
    getWxCode() {
      return new Promise((resolve, reject) => {
        uni.login({
          provider: 'weixin',
          success: (res) => (res.code ? resolve(res.code) : reject(new Error('未获取到微信授权'))),
          fail: () => reject(new Error('微信登录失败')),
        })
      })
    },
    async submit() {
      if (!this.username || !this.password) {
        uni.showToast({ title: '请输入账号和密码', icon: 'none' })
        return
      }
      this.loading = true
      try {
        let session
        if (this.isMock) {
          session = await api.loginMock(this.username, this.password)
        } else {
          const code = await this.getWxCode()
          const r = await api.wxLogin(code)
          if (r && r.token) {
            session = r
          } else if (r && r.need_bind) {
            session = await api.bind(r.openid_token, this.username, this.password)
          } else {
            throw new Error('登录失败')
          }
        }
        applySession(session)
        // 登录成功后拉一次订阅模板 ID（优先接口，失败降级 env）
        refreshTemplateIds()
        // 订阅消息深链：登录成功后消费待跳转的告警，再按角色落地。
        const pending = store.pendingAlertId
        store.pendingAlertId = ''   // 读到即清，防止泄漏到下次
        if (pending) {
          uni.reLaunch({ url: '/pages/alerts/detail?id=' + encodeURIComponent(pending) })
          return
        }
        // 教师落地「我的教室」，管理员落地总览首页。
        uni.reLaunch({ url: store.role === 'teacher' ? '/pages/classroom/classroom' : '/pages/home/home' })
      } catch (e) {
        uni.showToast({ title: e.message || '登录失败', icon: 'none' })
      } finally {
        this.loading = false
      }
    },
  },
}
</script>

<style scoped>
.login {
  min-height: 100vh;
  padding: 0 48rpx;
  box-sizing: border-box;
  background: var(--bg);
}
.hero {
  display: flex;
  flex-direction: column;
  align-items: center;
  padding: 120rpx 0 72rpx;
}
.logo {
  width: 120rpx;
  height: 120rpx;
  display: flex;
  align-items: center;
  justify-content: center;
  background: var(--primary);
  color: #fff;
  border-radius: 12rpx;
}
.name {
  margin-top: 32rpx;
  font-size: 42rpx;
  font-weight: 700;
  color: var(--text-1);
}
.slogan {
  margin-top: 12rpx;
  font-size: 26rpx;
  color: var(--text-3);
}
.panel {
  padding: 40rpx 36rpx;
}
.tip {
  display: block;
  margin-bottom: 28rpx;
  font-size: 24rpx;
  color: var(--text-3);
  text-align: center;
}
.field {
  height: 92rpx;
  margin-top: 20rpx;
  padding: 0 28rpx;
  background: #fff;
  border: 1rpx solid var(--border);
  border-radius: 8rpx;
  font-size: 30rpx;
}
.ph {
  color: var(--text-4);
}
.submit {
  margin-top: 36rpx;
}
.footer {
  display: block;
  margin-top: 48rpx;
  text-align: center;
  font-size: 22rpx;
  color: var(--text-4);
}
</style>
