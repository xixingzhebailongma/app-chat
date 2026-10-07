<template>
  <div class="bind">
    <div class="hero">
      <div class="logo">校</div>
      <div class="name">绑定孩子</div>
      <div class="slogan">绑定后，孩子到校将通过微信通知您</div>
    </div>

    <!-- 进入中 -->
    <div v-if="step === 'loading'" class="panel card">
      <div class="tip">正在进入…</div>
    </div>

    <!-- 授权/配置失败 -->
    <div v-else-if="step === 'error'" class="panel card">
      <div class="tip danger">{{ errorMsg }}</div>
      <button class="btn btn-primary" @click="retry">重试</button>
    </div>

    <!-- 绑定表单 -->
    <div v-else-if="step === 'form'" class="panel card">
      <div v-if="!bindings.length" class="empty-hint">尚无绑定记录，请填写信息完成首次绑定</div>
      <input v-model="studentNo" class="field" placeholder="孩子学号" />
      <input v-model="phone" class="field" type="tel" maxlength="11" placeholder="家长手机号" />
      <div class="sms-row">
        <input v-model="smsCode" class="field sms-field" type="text" inputmode="numeric" maxlength="6" placeholder="短信验证码" />
        <button class="btn btn-ghost sms-btn" :disabled="countdown > 0 || sending" @click="sendCode">
          {{ countdown > 0 ? countdown + 's' : '发送验证码' }}
        </button>
      </div>
      <div v-if="hint" class="hint">{{ hint }}</div>
      <div v-if="errorMsg" class="error">{{ errorMsg }}</div>
      <button class="btn btn-primary submit" :disabled="submitting" @click="submit">
        {{ submitting ? '绑定中…' : '绑定' }}
      </button>
    </div>

    <!-- 已绑定列表 -->
    <div v-else-if="step === 'bound'" class="panel card bound-panel">
      <div class="bound-title">已绑定的孩子</div>
      <div v-for="b in bindings" :key="b.student_no" class="child-card">
        <div class="child-info">
          <div class="child-name">{{ b.student_name || b.student_no }}</div>
          <div class="child-meta">学号 {{ b.student_no }} · {{ b.phone }}</div>
          <div class="child-meta muted">绑定于 {{ fmtBoundAt(b.bound_at) }}</div>
        </div>
        <button class="btn btn-ghost child-del" :disabled="removing" @click="removeBinding(b.student_no)">删除</button>
      </div>
      <button class="btn btn-primary add-btn" @click="addAnother">+ 绑定另一个孩子</button>
    </div>

    <div class="footer">智慧校园数字基座 · 学校家长服务</div>
  </div>
</template>

<script>
import config from '../config'
import { api, setUnauthorizedHandler } from '../api'

export default {
  name: 'Bind',
  data() {
    return {
      step: 'loading', // loading | form | bound | error
      openidToken: '',
      bindings: [],
      studentNo: '',
      phone: '',
      smsCode: '',
      countdown: 0,
      sending: false,
      submitting: false,
      removing: false,
      hint: '',
      errorMsg: '',
      timer: null,
    }
  },
  created() {
    setUnauthorizedHandler(() => this.reauth())
  },
  mounted() {
    if (config.USE_MOCK) {
      // mock：跳过真实网页授权，直接给假 openid_token。
      this.authorize()
      return
    }
    const qs = new URLSearchParams(location.search)
    const code = qs.get('code')
    const state = qs.get('state')
    const err = qs.get('error')
    const errDesc = qs.get('error_description')
    const saved = this.safeGetState()
    if (err) {
      // 微信网页授权被拒/失败：不回跳，避免无限重定向。
      this.fail(err === 'access_denied' ? '您已取消授权，无法继续绑定' : '授权失败：' + (errDesc || err))
      return
    }
    if (code) {
      // 有 code 必须校验 state：saved 缺失或 state 不匹配都判失败（防 CSRF）。
      if (!saved || state !== saved) {
        this.fail('授权校验失败，请重试')
        return
      }
      this.safeRemoveState()
      this.authorize(code)
    } else if (saved) {
      // 已发起过授权（有 state）却未带回 code，视为未完成授权，不再回跳。
      this.fail('未完成授权，请重试')
      this.safeRemoveState()
    } else {
      this.redirectToWechat()
    }
  },
  beforeUnmount() {
    if (this.timer) clearInterval(this.timer)
  },
  methods: {
    redirectToWechat() {
      if (!config.OA_APPID) {
        this.fail('未配置公众号 appid（VITE_OA_APPID）')
        return
      }
      const redirectUri = config.OA_REDIRECT_URI || location.origin + location.pathname
      const state = 'st_' + Math.random().toString(36).slice(2)
      this.safeSetState(state)
      const url =
        'https://open.weixin.qq.com/connect/oauth2/authorize' +
        '?appid=' + encodeURIComponent(config.OA_APPID) +
        '&redirect_uri=' + encodeURIComponent(redirectUri) +
        '&response_type=code' +
        '&scope=snsapi_base' +
        '&state=' + encodeURIComponent(state) +
        '#wechat_redirect'
      location.href = url
    },
    async authorize(code) {
      try {
        const r = await api.authorize(code)
        this.openidToken = r.openid_token
        this.errorMsg = ''
        await this.loadBindings()
      } catch (e) {
        this.fail(e.message || '授权失败')
      }
    },
    // 查当前 openid 的绑定列表，有则进已绑定列表，无则进表单。
    async loadBindings() {
      try {
        const r = await api.me(this.openidToken)
        this.bindings = r.bindings || []
        this.step = this.bindings.length > 0 ? 'bound' : 'form'
      } catch (e) {
        this.fail(e.message || '查询绑定失败')
      }
    },
    fail(msg) {
      this.errorMsg = msg
      this.step = 'error'
    },
    retry() {
      this.errorMsg = ''
      this.step = 'loading'
      if (config.USE_MOCK) {
        this.authorize()
      } else {
        this.redirectToWechat()
      }
    },
    reauth() {
      if (config.USE_MOCK) {
        this.authorize()
      } else {
        this.redirectToWechat()
      }
    },
    sendCode() {
      const phone = this.phone.trim()
      if (!/^1\d{10}$/.test(phone)) {
        this.errorMsg = '请输入 11 位手机号'
        return
      }
      if (this.countdown > 0 || this.sending) return
      this.sending = true
      this.errorMsg = ''
      api.sendCode(phone)
        .then(() => {
          this.countdown = 60
          this.timer = setInterval(() => {
            this.countdown -= 1
            if (this.countdown <= 0) {
              clearInterval(this.timer)
              this.timer = null
            }
          }, 1000)
          if (config.USE_MOCK) {
            this.hint = '开发验证码：' + config.DEV_SMS_CODE
          }
        })
        .catch((e) => {
          this.errorMsg = e.message || '发送失败'
        })
        .finally(() => {
          this.sending = false
        })
    },
    async submit() {
      if (this.submitting) return
      const studentNo = this.studentNo.trim()
      const phone = this.phone.trim()
      const smsCode = String(this.smsCode || '').trim()
      if (!studentNo) { this.errorMsg = '请输入学号'; return }
      if (!/^1\d{10}$/.test(phone)) { this.errorMsg = '请输入 11 位手机号'; return }
      if (!smsCode) { this.errorMsg = '请输入验证码'; return }
      if (this.bindings.some((b) => b.student_no === studentNo)) {
        this.errorMsg = '该学号已绑定，无需重复绑定'
        return
      }
      this.submitting = true
      this.errorMsg = ''
      try {
        await api.confirm(this.openidToken, studentNo, phone, smsCode)
        this.clearForm()
        await this.loadBindings()
      } catch (e) {
        if (e.status === 401) return // 401 已由拦截器触发 reauth，这里跳过提示
        this.errorMsg = e.message || '绑定失败'
      } finally {
        this.submitting = false
      }
    },
    async removeBinding(studentNo) {
      if (this.removing) return
      if (!window.confirm('确认删除该孩子的绑定？')) return
      this.removing = true
      try {
        await api.unbind(this.openidToken, studentNo)
        await this.loadBindings()
      } catch (e) {
        this.errorMsg = e.message || '删除失败'
      } finally {
        this.removing = false
      }
    },
    addAnother() {
      this.clearForm()
      this.step = 'form'
    },
    clearForm() {
      this.studentNo = ''
      this.phone = ''
      this.smsCode = ''
      this.hint = ''
      this.errorMsg = ''
      if (this.timer) {
        clearInterval(this.timer)
        this.timer = null
      }
      this.countdown = 0
    },
    // sessionStorage 在隐私模式/被禁用时会抛错，统一 try/catch 包裹（oa_state 仅作 CSRF 兜底）。
    safeGetState() {
      try { return sessionStorage.getItem('oa_state') } catch (e) { return null }
    },
    safeSetState(state) {
      try { sessionStorage.setItem('oa_state', state) } catch (e) { /* ignore */ }
    },
    safeRemoveState() {
      try { sessionStorage.removeItem('oa_state') } catch (e) { /* ignore */ }
    },
    fmtBoundAt(s) {
      if (!s) return '—'
      // 后端返回 ISO，裁成可读的 YYYY-MM-DD HH:mm；mock 已是该格式则原样。
      const m = String(s).match(/^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2})/)
      return m ? `${m[1]}-${m[2]}-${m[3]} ${m[4]}:${m[5]}` : s
    },
  },
}
</script>

<style scoped>
.bind {
  max-width: 480px;
  margin: 0 auto;
  padding: 0 20px;
}
.hero {
  display: flex;
  flex-direction: column;
  align-items: center;
  padding: 56px 0 32px;
}
.logo {
  width: 56px;
  height: 56px;
  display: flex;
  align-items: center;
  justify-content: center;
  background: var(--primary);
  color: #fff;
  border-radius: 12px;
  font-size: 28px;
  font-weight: 700;
}
.name {
  margin-top: 16px;
  font-size: 20px;
  font-weight: 700;
  color: var(--text-1);
}
.slogan {
  margin-top: 6px;
  font-size: 13px;
  color: var(--text-3);
}
.panel {
  padding: 24px 20px;
}
.tip {
  text-align: center;
  font-size: 14px;
  color: var(--text-3);
  padding: 20px 0;
}
.tip.danger {
  color: var(--danger);
}
.field {
  width: 100%;
  height: 46px;
  margin-top: 12px;
  padding: 0 14px;
  background: #fff;
  border: 1px solid var(--border);
  border-radius: 8px;
  font-size: 15px;
  outline: none;
}
.field:focus {
  border-color: var(--primary);
}
.empty-hint {
  margin-bottom: 4px;
  font-size: 13px;
  color: var(--text-3);
}
.sms-row {
  display: flex;
  gap: 10px;
  margin-top: 12px;
}
.sms-field {
  flex: 1;
  margin-top: 0;
}
.sms-btn {
  width: 116px;
  flex-shrink: 0;
  height: 46px;
  font-size: 14px;
}
.hint {
  margin-top: 12px;
  font-size: 13px;
  color: var(--warning);
}
.error {
  margin-top: 12px;
  font-size: 13px;
  color: var(--danger);
}
.submit {
  margin-top: 20px;
}
.bound-panel {
  padding: 20px;
}
.bound-title {
  font-size: 16px;
  font-weight: 700;
  color: var(--text-1);
  margin-bottom: 16px;
}
.child-card {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 14px;
  border: 1px solid var(--border);
  border-radius: 10px;
  margin-bottom: 10px;
  background: #fff;
}
.child-info {
  min-width: 0;
}
.child-name {
  font-size: 16px;
  font-weight: 600;
  color: var(--text-1);
}
.child-meta {
  margin-top: 4px;
  font-size: 13px;
  color: var(--text-2);
}
.child-meta.muted {
  color: var(--text-3);
}
.child-del {
  flex-shrink: 0;
  margin-left: 12px;
  width: auto;
  height: 32px;
  padding: 0 12px;
  font-size: 13px;
}
.add-btn {
  width: 100%;
  margin-top: 8px;
}
.footer {
  margin-top: 40px;
  text-align: center;
  font-size: 12px;
  color: var(--text-4);
  padding-bottom: 24px;
}
</style>
