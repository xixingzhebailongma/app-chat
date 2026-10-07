import config from './config'

// 本地 mock：无公众号凭证时跑通「授权 → 表单 → 绑定 / 已绑定列表」全流程。
// 多孩子：用 localStorage 存一个绑定数组，模拟后端持久化，reload 后 me() 仍能命中。
const delay = (data, ms = 300) => new Promise((resolve) => setTimeout(() => resolve(data), ms))

const KEY = 'oa_bindings'

function readBindings() {
  try {
    const raw = localStorage.getItem(KEY)
    return raw ? JSON.parse(raw) : []
  } catch (e) {
    return []
  }
}

function writeBindings(list) {
  try {
    localStorage.setItem(KEY, JSON.stringify(list))
  } catch (e) {
    /* ignore */
  }
}

function now() {
  const d = new Date()
  const p = (n) => String(n).padStart(2, '0')
  return `${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())} ${p(d.getHours())}:${p(d.getMinutes())}`
}

function placeholderName(studentNo) {
  return '学生' + String(studentNo).slice(-3)
}

export const mockApi = {
  authorize() {
    return delay({ openid: 'mock_oa_openid', openid_token: 'mock-openid-token' })
  },
  sendCode(phone) {
    return delay({ sent: true })
  },
  // 查当前 openid 的绑定列表（多孩子）。openidToken 仅签名对齐真实接口，mock 不校验。
  me(openidToken) {
    const bindings = readBindings()
    return delay({ bound: bindings.length > 0, bindings })
  },
  // 删除指定学号的绑定。
  unbind(openidToken, studentNo) {
    writeBindings(readBindings().filter((b) => b.student_no !== studentNo))
    return delay({ unbound: true })
  },
  confirm(openidToken, studentNo, phone, smsCode) {
    if (smsCode !== config.DEV_SMS_CODE) {
      return delay(null, 200).then(() =>
        Promise.reject(Object.assign(new Error('验证码错误'), { status: 400 }))
      )
    }
    const boundAt = now()
    // 同 student_no 已绑定时替换（重绑=更新），避免重复 key（Bind.vue :key="b.student_no"）。
    writeBindings(
      readBindings()
        .filter((b) => b.student_no !== studentNo)
        .concat({
          student_no: studentNo,
          student_name: placeholderName(studentNo),
          phone,
          bound_at: boundAt,
        })
    )
    return delay({ bound: true, student_no: studentNo, phone, bound_at: boundAt })
  },
}
