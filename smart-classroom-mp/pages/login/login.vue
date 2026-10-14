<template>
  <view class="login">
    <!-- 顶部渐变 Hero（引导页允许渐变；图标一律 app-icon，不用 emoji） -->
    <view class="hero">
      <view class="hero__circle hero__circle--1"></view>
      <view class="hero__circle hero__circle--2"></view>
      <view class="hero__brand">
        <app-icon name="room" :size="120" :color="COLORS.white" class="hero__logo" />
        <text class="hero__title">智慧教室</text>
        <text class="hero__sub">移动管理终端 · 教师 / 管理员</text>
      </view>
    </view>

    <!-- 登录卡片 -->
    <view class="panel">
      <view class="field" :class="{ 'field--err': !!errMsg }">
        <app-icon name="user" :size="36" :color="COLORS.text3" class="field__icon" />
        <input
          v-model="username"
          class="field__input"
          type="text"
          placeholder="请输入账号"
          placeholder-class="field__ph"
          confirm-type="next"
        />
      </view>
      <view class="field" :class="{ 'field--err': !!errMsg }">
        <app-icon name="lock" :size="36" :color="COLORS.text3" class="field__icon" />
        <input
          v-model="password"
          class="field__input"
          type="password"
          password
          placeholder="请输入密码"
          placeholder-class="field__ph"
          confirm-type="done"
          @confirm="doLogin"
        />
      </view>

      <view v-if="errMsg" class="err">{{ errMsg }}</view>

      <button class="c-btn c-btn--primary login__btn" :disabled="loading" @tap="doLogin">
        {{ loading ? '登录中…' : '登 录' }}
      </button>

      <view v-if="mockOn" class="hint">
        <text class="hint__title">演示模式</text>
        <text class="hint__line">教师端：teacher / 123456</text>
        <text class="hint__line">管理端：admin / 123456</text>
      </view>
    </view>

    <text class="foot">告警实时推送 · 远程设备控制 · 云边协同</text>
  </view>
</template>

<script setup>
import { ref, computed } from 'vue'
import { login } from '../../api'
import { USE_MOCK } from '../../api/config'
import { saveAuth } from '../../store'
import { roleHome } from '../../utils/permission'
import { COLORS } from '../../config/theme'

const username = ref('')
const password = ref('')
const loading = ref(false)
const errMsg = ref('')
const mockOn = computed(() => USE_MOCK)

async function doLogin() {
  if (loading.value) return
  if (!username.value.trim() || !password.value) {
    errMsg.value = '请输入账号和密码'
    return
  }
  loading.value = true
  errMsg.value = ''
  const { data, error } = await login(username.value.trim(), password.value)
  loading.value = false
  if (error || !data || !data.user) {
    errMsg.value = (error && error.message) || '登录失败，请重试'
    return
  }
  // space_id / spaces 由云端根据登录态在响应中返回，本地不硬编码
  saveAuth(data.token, data.user, data)
  uni.reLaunch({ url: roleHome(data.user) })
}
</script>

<style lang="scss" scoped>
.login {
  min-height: 100vh;
  background: $bg;
  display: flex;
  flex-direction: column;
}
.hero {
  position: relative;
  height: 420rpx;
  background: $grad-brand;
  border-radius: 0 0 60rpx 60rpx;
  overflow: hidden;
  display: flex;
  align-items: center;
  justify-content: center;
}
.hero__circle {
  position: absolute;
  border-radius: 50%;
  background: rgba(255, 255, 255, 0.10);
}
.hero__circle--1 { width: 320rpx; height: 320rpx; top: -110rpx; right: -80rpx; }
.hero__circle--2 { width: 200rpx; height: 200rpx; bottom: -60rpx; left: -50rpx; }
.hero__brand { display: flex; flex-direction: column; align-items: center; }
.hero__logo { margin-bottom: 8rpx; }
.hero__title { color: $bg-card; font-size: 52rpx; font-weight: 700; margin-top: 8rpx; letter-spacing: 4rpx; }
.hero__sub { color: rgba(255, 255, 255, 0.8); font-size: $fs-sm; margin-top: 10rpx; }

.panel {
  margin: -50rpx 40rpx 0;
  background: $card;
  border-radius: $radius;
  box-shadow: $shadow-float;
  padding: 48rpx 40rpx;
  position: relative;
}
.field {
  display: flex;
  align-items: center;
  background: $disabled-light;
  border-radius: $radius-sm;
  padding: 0 24rpx;
  height: 100rpx;
  margin-bottom: 28rpx;
  border: 2rpx solid transparent;
  transition: border-color 0.2s;
}
.field--err { border-color: $danger-50; }
.field__icon { margin-right: 18rpx; flex: none; }
.field__input { flex: 1; height: 100rpx; font-size: $fs-md; }
.field__ph { color: $text-3; }
.err { color: $danger; font-size: $fs-sm; margin: -8rpx 0 20rpx; }
.login__btn { width: 100%; margin-top: 8rpx; }

.hint {
  margin-top: 32rpx;
  background: $primary-light;
  border-radius: $radius-sm;
  padding: 20rpx 24rpx;
  display: flex;
  flex-direction: column;
}
.hint__title { font-size: $fs-sm; color: $primary; font-weight: 600; margin-bottom: 6rpx; }
.hint__line { font-size: $fs-xs; color: $text-2; line-height: 36rpx; }

.foot {
  text-align: center;
  color: $text-3;
  font-size: $fs-xs;
  margin-top: 60rpx;
}
</style>
