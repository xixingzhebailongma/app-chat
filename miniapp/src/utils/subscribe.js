import config from '../config'
import { api } from '../api/index'

// 订阅授权共享逻辑（profile 开关 + 落地页引导条共用）。
// 微信要求 uni.requestSubscribeMessage 必须在用户点击的同步调用栈内调用，
// 因此 requestSubscribe 内部不做任何 await，调用方须在 tap 处理器里直接调用。

// 本次会话内「暂不」关闭订阅引导的标记（模块级，随 App 生命周期存在）。
let dismissed = false

// 已从网关拉取的订阅模板 ID（模块级缓存）；null = 尚未拉取，此时降级 env。
let fetchedTmplIds = null

export function promptDismissed() {
  return dismissed
}

export function dismissPrompt() {
  dismissed = true
}

// 从网关拉权威模板 ID（GET /api/miniapp/notify/templates）。
// 成功后缓存；失败静默降级到 env 的 SUBSCRIBE_TMPL_IDS。登录后调用一次。
export async function refreshTemplateIds() {
  try {
    const r = await api.notifyTemplates()
    const ids = (r && r.template_ids) || []
    if (ids.length) fetchedTmplIds = ids
  } catch (e) {
    // 接口失败：保留 env 降级，不打扰用户
  }
}

// 拉起微信订阅授权。返回 Promise<result>，
// result ∈ 'accepted' | 'banned' | 'rejected' | 'failed' | 'skipped'。
export function requestSubscribe() {
  return new Promise((resolve) => {
    if (config.USE_MOCK) {
      resolve('skipped')
      return
    }
    const tmplIds = (fetchedTmplIds && fetchedTmplIds.length)
      ? fetchedTmplIds
      : (config.SUBSCRIBE_TMPL_IDS || [])
    if (!tmplIds.length) {
      uni.showToast({ title: '未配置订阅模板 ID', icon: 'none' })
      resolve('skipped')
      return
    }
    // 微信单次最多 3 个模板：超出截断丢弃并告警，防止未来模板数超 3 时静默失败。
    const ids = tmplIds.slice(0, 3)
    if (tmplIds.length > 3) {
      console.warn('[subscribe] 订阅模板超过 3 个，已截断至前 3 个：', tmplIds)
    }
    uni.requestSubscribeMessage({
      tmplIds: ids,
      success: (res) => {
        const accepted = []
        let banned = false
        Object.keys(res || {}).forEach((tid) => {
          const status = res[tid]
          if (status === 'accept') accepted.push(tid)
          else if (status === 'ban') banned = true
          // reject / filter：静默忽略，状态由 GET 回显兜底。
        })
        if (accepted.length) {
          api.subscribe({ accepted_templates: accepted })
            .then(() => uni.showToast({ title: '已开启订阅', icon: 'none' }))
            .catch((err) => uni.showToast({ title: err.message || '操作失败', icon: 'none' }))
          resolve('accepted')
          return
        }
        uni.showToast({
          title: banned ? '已禁止，请到微信「设置」开启' : '未开启订阅',
          icon: 'none',
        })
        resolve(banned ? 'banned' : 'rejected')
      },
      fail: () => {
        uni.showToast({ title: '拉起授权失败', icon: 'none' })
        resolve('failed')
      },
    })
  })
}

// 是否已开启任一订阅模板（真实模式查网关；演示模式恒 false）。
export async function subscriptionActive() {
  if (config.USE_MOCK) return false
  try {
    const r = await api.getSubscriptions()
    const templates = (r && r.templates) || {}
    return Object.values(templates).some((t) => t && t.active)
  } catch (e) {
    return false
  }
}
