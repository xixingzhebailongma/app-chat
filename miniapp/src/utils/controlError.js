// 设备/控制端点错误文案映射（设备模块内，不动全局 request.js）。
// 与 alertError.js 同思路：按网关标准信封的 err.code 映射，其余回退原始 message。
const CONTROL_CODE_MESSAGES = {
  FORBIDDEN: '无权限操作',
  INVALID_REQUEST: '参数错误',
  NOT_FOUND: '对象不存在',
  GO_BACKEND_UNAVAILABLE: '设备服务暂不可用',
}

// 把 request 抛出的 err 转成可展示文案；未命中的 code 回退原始 message（可能空串，由调用方兜底）。
// 注：门禁 off 的 428 CONFIRM_REQUIRED 不在此处理，由 devices.vue 用二次确认弹窗单独拦截。
export function controlErrorMessage(err) {
  if (err && err.code && CONTROL_CODE_MESSAGES[err.code]) {
    return CONTROL_CODE_MESSAGES[err.code]
  }
  return (err && err.message) || ''
}
