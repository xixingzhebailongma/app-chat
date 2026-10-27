<template>
  <view class="ai">
    <!-- 顶栏：标题 + 新对话（有消息时显示） -->
    <view class="ai-header">
      <text class="ai-header-t">AI 助手</text>
      <view v-if="messages.length" class="new-chat" @tap="newChat">＋ 新对话</view>
    </view>

    <!-- 空态：欢迎 + 能力说明 + 快捷问题（有消息后隐藏，给对话让位） -->
    <view v-if="!messages.length" class="welcome">
      <view class="welcome-logo">AI</view>
      <text class="welcome-t">你好，我是教室 AI 助手</text>
      <text class="welcome-d">可查设备 / 告警 / 进出记录 / 场景，可执行场景与单设备开关；门禁开门需到小程序二次确认。</text>
      <scroll-view scroll-x class="quick" :show-scrollbar="false">
        <view class="quick-row">
          <view v-for="q in quickQuestions" :key="q" class="quick-chip" @tap="quickAsk(q)">{{ q }}</view>
        </view>
      </scroll-view>
    </view>

    <!-- 消息列表 -->
    <scroll-view scroll-y class="chat" :scroll-into-view="scrollTo" scroll-with-animation>
      <view
        v-for="(m, i) in messages"
        :key="i"
        :id="'msg-' + i"
        class="msg"
        :class="m.role === 'user' ? 'msg-user' : 'msg-ai'"
      >
        <view v-if="m.role === 'assistant'" class="avatar">AI</view>
        <view class="bubble">{{ m.content }}</view>
      </view>
      <view v-if="loading" id="msg-loading" class="msg msg-ai">
        <view class="avatar">AI</view>
        <view class="bubble bubble-thinking">思考中…</view>
      </view>
    </scroll-view>

    <!-- 输入框（DeepSeek 风：自适应高度的 textarea + 圆形发送键） -->
    <view class="composer">
      <textarea
        v-model="draft"
        class="composer-input"
        auto-height
        :show-confirm-bar="false"
        confirm-type="send"
        placeholder="问点什么，例如「A101 有哪些设备在线」"
        placeholder-class="ph"
        :disabled="loading"
        @confirm="send"
      />
      <view class="send" :class="{ active: draft.trim() && !loading }" @tap="send">
        <text class="send-arrow">↑</text>
      </view>
    </view>

    <AppTabBar current="ai" />
  </view>
</template>

<script>
import { api } from '../../api/index'
import { store, isLoggedIn, saveAiConversationId } from '../../store/index'
import AppTabBar from '../../components/AppTabBar.vue'

export default {
  components: { AppTabBar },
  data() {
    return {
      loading: false,
      scrollTo: '',
      quickQuestions: [
        '列出我能访问的教室',
        'A101 有哪些设备在线？',
        '最近有什么告警？',
        '今天的进出记录',
        'A101 有哪些场景？',
        '执行 A101 的开启模式',
      ],
    }
  },
  computed: {
    draft: {
      get() { return store.aiDraft },
      set(v) { store.aiDraft = v },
    },
    messages: {
      get() { return store.aiMessages },
      set(v) { store.aiMessages = v },
    },
  },
  onShow() {
    if (!isLoggedIn()) {
      uni.reLaunch({ url: '/pages/login/login' })
      return
    }
    this.restoreHistory()
  },
  methods: {
    quickAsk(q) {
      this.draft = q
      this.send()
    },
    newChat() {
      saveAiConversationId('')
      store.aiMessages = []
    },
    async restoreHistory() {
      if (store.aiMessages.length) return // 已有消息不重复拉取
      // 已有会话 id：拉历史回填（跨刷新）。
      if (store.aiConversationId) {
        try {
          const res = await api.aiMessages(store.aiConversationId)
          const msgs = (res && res.messages) || []
          store.aiMessages = msgs
            .filter((m) => m.role === 'user' || m.role === 'assistant')
            .map((m) => ({ role: m.role, content: m.content }))
        } catch (e) {
          saveAiConversationId('')
          store.aiMessages = []
        }
        return
      }
      // 无会话 id：续最近一个会话（跨设备续聊）。
      try {
        const res = await api.aiConversations()
        const list = (res && res.conversations) || []
        if (list.length) {
          saveAiConversationId(list[0].conversation_id)
          await this.restoreHistory()
        }
      } catch (e) {}
    },
    pushMessage(m) {
      const MAX = 50
      this.messages.push(m)
      if (this.messages.length > MAX) {
        this.messages.splice(0, this.messages.length - MAX)
      }
    },
    scrollToBottom() {
      this.$nextTick(() => {
        this.scrollTo = this.loading ? 'msg-loading' : 'msg-' + (this.messages.length - 1)
      })
    },
    async send() {
      const text = this.draft.trim()
      if (!text || this.loading) return
      this.draft = ''
      this.pushMessage({ role: 'user', content: text })
      this.loading = true
      this.scrollToBottom()

      try {
        await this.ask(text, store.aiConversationId)
      } catch (e) {
        // 会话失效（如网关重启、内存模式历史清空）：清掉旧 id，新会话重试一次。
        if (e.status === 404) {
          saveAiConversationId('')
          try {
            await this.ask(text, '')
          } catch (e2) {
            this.pushMessage({ role: 'assistant', content: '请求失败：' + (e2.message || '未知错误') })
          }
        } else {
          this.pushMessage({ role: 'assistant', content: '请求失败：' + (e.message || '未知错误') })
        }
      } finally {
        this.loading = false
        this.scrollToBottom()
      }
    },
    async ask(text, conversationId) {
      // 只发本轮新消息 + 会话 id；历史由服务端记忆。
      const res = await api.aiChat([{ role: 'user', content: text }], conversationId)
      if (res && res.conversation_id) saveAiConversationId(res.conversation_id)
      const reply = (res && res.reply) || '(空回复)'
      this.pushMessage({ role: 'assistant', content: reply })
    },
  },
}
</script>

<style scoped>
.ai {
  height: 100vh;
  display: flex;
  flex-direction: column;
  box-sizing: border-box;
  padding-bottom: calc(140rpx + env(safe-area-inset-bottom));
}

/* 顶栏 */
.ai-header {
  flex-shrink: 0;
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 20rpx 24rpx 8rpx;
}
.ai-header-t {
  font-size: 32rpx;
  font-weight: 600;
  color: var(--text-1);
}
.new-chat {
  font-size: 24rpx;
  color: var(--primary);
  background: var(--primary-soft);
  border: 1rpx solid var(--primary-border);
  border-radius: 30rpx;
  padding: 6rpx 20rpx;
}
.new-chat:active {
  background: rgba(91, 141, 239, 0.18);
}

/* 空态欢迎区 */
.welcome {
  flex-shrink: 0;
  display: flex;
  flex-direction: column;
  align-items: center;
  text-align: center;
  padding: 100rpx 40rpx 8rpx;
}
.welcome-logo {
  width: 96rpx;
  height: 96rpx;
  border-radius: 24rpx;
  background: var(--primary);
  color: #ffffff;
  font-size: 40rpx;
  font-weight: 600;
  display: flex;
  align-items: center;
  justify-content: center;
}
.welcome-t {
  margin-top: 24rpx;
  font-size: 34rpx;
  font-weight: 600;
  color: var(--text-1);
}
.welcome-d {
  margin-top: 12rpx;
  font-size: 24rpx;
  color: var(--text-3);
  line-height: 1.6;
}

/* 快捷问题 */
.quick {
  width: 100%;
  margin-top: 32rpx;
  white-space: nowrap;
}
.quick-row {
  display: inline-flex;
  padding: 4rpx 2rpx;
}
.quick-chip {
  flex: 0 0 auto;
  height: 60rpx;
  padding: 0 28rpx;
  margin-right: 16rpx;
  display: flex;
  align-items: center;
  font-size: 24rpx;
  color: var(--primary);
  background: var(--primary-soft);
  border: 1rpx solid var(--primary-border);
  border-radius: 30rpx;
}
.quick-chip:active {
  background: rgba(91, 141, 239, 0.18);
}

/* 消息区 */
.chat {
  flex: 1;
  min-height: 0;
  padding: 20rpx 24rpx 8rpx;
}
.msg {
  display: flex;
  align-items: flex-start;
  margin-bottom: 28rpx;
}
.msg-user {
  justify-content: flex-end;
}
.msg-ai {
  justify-content: flex-start;
}
.avatar {
  flex-shrink: 0;
  width: 64rpx;
  height: 64rpx;
  margin-right: 16rpx;
  border-radius: 16rpx;
  background: var(--primary);
  color: #ffffff;
  font-size: 24rpx;
  font-weight: 600;
  display: flex;
  align-items: center;
  justify-content: center;
}
.bubble {
  font-size: 28rpx;
  line-height: 1.6;
  white-space: pre-wrap;
  word-break: break-word;
}
.msg-ai .bubble {
  max-width: 76%;
  padding: 8rpx 0 0;
  color: var(--text-1);
}
.msg-user .bubble {
  max-width: 72%;
  padding: 18rpx 24rpx;
  background: var(--primary-soft);
  color: var(--text-1);
  border: 1rpx solid var(--primary-border);
  border-radius: 20rpx 6rpx 20rpx 20rpx;
}
.bubble-thinking {
  color: var(--text-3);
}

/* 输入框（DeepSeek 风） */
.composer {
  flex-shrink: 0;
  display: flex;
  align-items: flex-end;
  margin: 12rpx 24rpx 12rpx;
  padding: 16rpx 16rpx 16rpx 24rpx;
  background: rgba(255, 255, 255, 0.92);
  border: 1rpx solid var(--border-light);
  border-radius: 32rpx;
  box-shadow: var(--shadow);
}
.composer-input {
  flex: 1;
  min-height: 44rpx;
  max-height: 240rpx;
  font-size: 28rpx;
  line-height: 1.5;
  color: var(--text-1);
}
.ph {
  color: var(--text-4);
}
.send {
  flex-shrink: 0;
  width: 60rpx;
  height: 60rpx;
  margin-left: 16rpx;
  border-radius: 50%;
  background: var(--text-4);
  color: #ffffff;
  display: flex;
  align-items: center;
  justify-content: center;
  transition: background 0.2s;
}
.send.active {
  background: var(--primary);
}
.send-arrow {
  font-size: 36rpx;
  font-weight: 700;
  line-height: 1;
}
</style>
