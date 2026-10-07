// 告警端点错误文案映射（仅告警范围，不动全局 request.js）。
// 网关标准错误信封 {"error":{"code","message"}}，request.js 已把 code 提取到 err.code；
// 这里只把告警端点会用到的 code 映射成中文，其余回退到原始 message。
const ALERT_CODE_MESSAGES = {
  FORBIDDEN: '无权限操作',
  INVALID_REQUEST: '参数错误',
}

// 把 request 抛出的 err 转成可展示文案；告警范围只覆盖 FORBIDDEN / INVALID_REQUEST，
// 其它错误返回原始 message（可能为空串，由调用方兜底）。
export function alertErrorMessage(err) {
  if (err && err.code && ALERT_CODE_MESSAGES[err.code]) {
    return ALERT_CODE_MESSAGES[err.code]
  }
  return (err && err.message) || ''
}
