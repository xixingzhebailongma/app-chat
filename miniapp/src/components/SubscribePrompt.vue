<template>
  <view v-if="visible" class="sub-prompt card">
    <view class="sp-main">
      <text class="sp-title">开启告警订阅</text>
      <text class="sp-desc">设备离线、告警等将通过微信订阅消息推送</text>
    </view>
    <view class="sp-actions">
      <text class="sp-btn enable" @tap="onEnable">开启</text>
      <text class="sp-btn dismiss" @tap="onDismiss">暂不</text>
    </view>
  </view>
</template>

<script>
import config from '../config'
import { promptDismissed, dismissPrompt, requestSubscribe, subscriptionActive } from '../utils/subscribe'

// 首开/登录落地页的订阅引导条：仅真实模式、未配置模板 ID 或已订阅/已关闭时不显示。
// 「开启」在 tap 同步栈内调用 requestSubscribeMessage；「暂不」关闭本次会话。
export default {
  name: 'SubscribePrompt',
  data() {
    return {
      visible: false,
    }
  },
  mounted() {
    this.init()
  },
  methods: {
    async init() {
      if (config.USE_MOCK) return
      if (!(config.SUBSCRIBE_TMPL_IDS || []).length) return
      if (promptDismissed()) return
      const active = await subscriptionActive()
      if (!active) this.visible = true
    },
    onEnable() {
      requestSubscribe().then(() => {
        this.visible = false
      })
    },
    onDismiss() {
      dismissPrompt()
      this.visible = false
    },
  },
}
</script>

<style scoped>
.sub-prompt {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 24rpx;
  margin-bottom: 20rpx;
  background: var(--warning-bg);
}
.sp-main {
  flex: 1;
  display: flex;
  flex-direction: column;
  margin-right: 20rpx;
}
.sp-title {
  font-size: 28rpx;
  font-weight: 600;
  color: var(--text-1);
}
.sp-desc {
  margin-top: 6rpx;
  font-size: 22rpx;
  color: var(--text-3);
}
.sp-actions {
  display: flex;
  align-items: center;
}
.sp-btn {
  font-size: 24rpx;
  padding: 10rpx 24rpx;
  border-radius: 8rpx;
}
.sp-btn.enable {
  color: #fff;
  background: var(--primary);
  margin-right: 16rpx;
}
.sp-btn.dismiss {
  color: var(--text-3);
}
</style>
