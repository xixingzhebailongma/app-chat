<template>
  <view class="login">
    <view class="hero">
      <text class="brand">智慧校园数字基座</text>
      <text class="slogan">设备状态 · 告警处理 · 空间管理</text>
    </view>

    <view class="form">
      <view class="field">
        <uni-icons type="person" size="20" color="var(--text-3)" />
        <input v-model="username" class="field-input" placeholder="请输入账号" placeholder-class="ph" />
      </view>
      <view class="field">
        <uni-icons type="locked" size="20" color="var(--text-3)" />
        <input v-model="password" class="field-input" :password="!showPassword" placeholder="请输入密码" placeholder-class="ph" />
        <view class="eye" @tap="showPassword = !showPassword">
          <uni-icons :type="showPassword ? 'eye-slash' : 'eye'" size="20" color="#245e70" />
        </view>
      </view>

      <view class="link-row">
        <text class="link-item">忘记密码？</text>
        <text class="link-item link-primary">还没有账号？立即注册</text>
      </view>

      <button class="btn btn-primary submit" :loading="loading" @tap="submit">{{ submitText }}</button>
    </view>

    <view class="divider"><text class="divider-text">其他登录方式</text></view>
    <view class="social">
      <view class="social-btn" hover-class="social-hover" @tap="onWxTap">
        <uni-icons type="weixin" size="22" color="#07C160" />
        <text class="social-text">微信登录</text>
      </view>
    </view>

    <!-- 开发联调直登：无正式微信 appid 时绕过 wx.login，直接使用预置开发 JWT（仅真实模式显示） -->
    <view v-if="!isMock" class="dev-login">
      <text class="dev-tip">开发联调直登（无需微信授权）</text>
      <view class="dev-btns">
        <button class="btn dev-btn" @tap="onDevLogin('teacher')">教师体验</button>
        <button class="btn dev-btn" @tap="onDevLogin('admin')">管理员体验</button>
      </view>
    </view>

    <text class="footer">© 智慧校园数字基座</text>
  </view>
</template>

<script>
import config from '../../config'
import { api } from '../../api/index'
import { store, applySession, isLoggedIn } from '../../store/index'
import { refreshTemplateIds } from '../../utils/subscribe'

// 开发联调直登 token：用网关 dev 密钥（dev-shared-jwt-secret）离线签发，2030 年到期。
// 仅用于本地真实后端联调；生产构建前需移除本区块（正式环境走 wx.login）。
const DEV_TOKENS = {
  teacher:
    'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ1c2VyX2lkIjoidV90ZWFjaGVyXzEiLCJyb2xlIjoidGVhY2hlciIsImV4cCI6MTkwMDAwMDAwMH0.AmM10Adj6UYhCWbDdc9M_MwRY0ZgtIprqnt1Xl3vMFw',
  admin:
    'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ1c2VyX2lkIjoidV9hZG1pbl8xIiwicm9sZSI6ImFkbWluIiwiZXhwIjoxOTAwMDAwMDAwfQ.OpqJZ7jG157Taj9E20tReR--5mZwgYfDk5Lw1HwvrGU',
}
const DEV_NAMES = { teacher: '李老师', admin: '管理员' }

export default {
  data() {
    return {
      isMock: config.USE_MOCK,
      username: '',
      password: '',
      showPassword: false,
      loading: false,
    }
  },
  computed: {
    submitText() {
      return this.isMock ? '登 录' : '登录 / 绑定'
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
    afterLogin() {
      const pending = store.pendingAlertId
      store.pendingAlertId = ''
      if (pending) {
        uni.reLaunch({ url: '/pages/alerts/detail?id=' + encodeURIComponent(pending) })
        return
      }
      uni.reLaunch({ url: store.role === 'teacher' ? '/pages/classroom/classroom' : '/pages/home/home' })
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
          applySession(session)
          try {
            const me = await api.me()
            if (me) {
              applySession({
                token: store.token,
                user_id: me.user_id || store.userId,
                role: me.role || store.role,
                name: me.name || store.name,
              })
            }
          } catch (e) {}
          session = null
        }
        if (session) applySession(session)
        refreshTemplateIds()
        this.afterLogin()
      } catch (e) {
        uni.showToast({ title: e.message || '登录失败', icon: 'none' })
      } finally {
        this.loading = false
      }
    },
    async onWxTap() {
      if (this.isMock) {
        uni.showToast({ title: '演示模式请用账号登录', icon: 'none' })
        return
      }
      if (this.loading) return
      this.loading = true
      try {
        const code = await this.getWxCode()
        const r = await api.wxLogin(code)
        if (r && r.token) {
          applySession(r)
          refreshTemplateIds()
          this.afterLogin()
        } else if (r && r.need_bind) {
          uni.showToast({ title: '请先用账号密码登录以绑定微信', icon: 'none' })
        } else {
          throw new Error('微信登录失败')
        }
      } catch (e) {
        uni.showToast({ title: e.message || '微信登录失败', icon: 'none' })
      } finally {
        this.loading = false
      }
    },
    // 开发联调直登：写入预置开发 JWT，直接进入真实后端会话（绕过 wx.login）。
    async onDevLogin(role) {
      if (this.loading) return
      this.loading = true
      try {
        const token = DEV_TOKENS[role]
        const userId = role === 'admin' ? 'u_admin_1' : 'u_teacher_1'
        applySession({ token, userId, role, name: DEV_NAMES[role] })
        try {
          const me = await api.me()
          if (me) {
            applySession({
              token,
              user_id: me.user_id || userId,
              role: me.role || role,
              name: me.name || DEV_NAMES[role],
            })
          }
        } catch (e) {}
        refreshTemplateIds()
        this.afterLogin()
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
  padding: 0 48rpx 48rpx;
  box-sizing: border-box;
  background: linear-gradient(180deg, var(--selected-bg) 0%, var(--bg-page) 560rpx, var(--bg-page) 100%);
  display: flex;
  flex-direction: column;
}
/* 标题区：中文主标题 + 灰色标语 */
.hero {
  padding: 160rpx 0 80rpx;
}
.brand {
  display: block;
  font-size: 56rpx;
  font-weight: 700;
  color: var(--text-1);
  letter-spacing: 2rpx;
}
.slogan {
  display: block;
  margin-top: 24rpx;
  font-size: 28rpx;
  color: var(--text-3);
  letter-spacing: 1rpx;
}
/* 表单：纯白底扁平输入框 + 极浅边框 */
.form {
  padding: 0;
}
.field {
  display: flex;
  align-items: center;
  height: 104rpx;
  padding: 0 32rpx;
  background: var(--bg-card);
  border: 1rpx solid var(--border-light);
  border-radius: 28rpx;
  margin-bottom: 32rpx;
  box-shadow: 0 4rpx 16rpx rgba(0, 0, 0, 0.03);
}
.field-input {
  flex: 1;
  margin-left: 20rpx;
  font-size: 30rpx;
  color: var(--text-1);
  background: transparent;
}
.ph {
  color: var(--text-4);
}
.eye {
  padding: 10rpx;
  margin-right: -10rpx;
}
/* 链接行：左「忘记密码」右「立即注册」 */
.link-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin: 8rpx 4rpx 0;
}
.link-item {
  font-size: 26rpx;
  color: var(--text-3);
}
.link-primary {
  color: var(--primary);
}
/* 主按钮：品牌蓝 + 蓝调阴影 */
.submit {
  margin-top: 56rpx;
  height: 104rpx;
  border-radius: var(--radius-pill);
  background: var(--brand-grad);
  box-shadow: 0 14rpx 28rpx rgba(36, 94, 112, 0.30);
  font-size: 32rpx;
  font-weight: 600;
  letter-spacing: 4rpx;
}
.divider {
  display: flex;
  align-items: center;
  margin: 88rpx 0 40rpx;
}
.divider::before,
.divider::after {
  content: '';
  flex: 1;
  height: 1rpx;
  background: var(--border);
}
.divider-text {
  margin: 0 24rpx;
  font-size: 24rpx;
  color: var(--text-3);
}
/* 社交按钮：全宽白色横条，与表单同宽 */
.social {
  display: flex;
}
.social-btn {
  flex: 1;
  display: flex;
  align-items: center;
  justify-content: center;
  height: 100rpx;
  border-radius: 28rpx;
  background: var(--bg-card);
  border: 1rpx solid var(--border-light);
  box-shadow: 0 4rpx 16rpx rgba(0, 0, 0, 0.03);
}
.social-text {
  margin-left: 14rpx;
  font-size: 28rpx;
  font-weight: 500;
  color: var(--text-1);
}
.social-hover {
  opacity: 0.85;
  transform: scale(0.98);
}
/* 开发联调直登：低调描边小按钮 */
.dev-login {
  margin-top: 40rpx;
}
.dev-tip {
  display: block;
  text-align: center;
  font-size: 24rpx;
  color: var(--text-4);
  margin-bottom: 20rpx;
}
.dev-btns {
  display: flex;
  gap: 24rpx;
}
.dev-btn {
  flex: 1;
  height: 80rpx;
  line-height: 80rpx;
  font-size: 26rpx;
  color: var(--primary);
  background: var(--bg-card);
  border: 1rpx solid var(--primary);
  border-radius: var(--radius-pill);
  padding: 0;
}
.dev-btn::after {
  border: none;
}
.footer {
  display: block;
  margin-top: auto;
  padding-top: 64rpx;
  text-align: center;
  font-size: 24rpx;
  color: var(--text-4);
}
</style>
