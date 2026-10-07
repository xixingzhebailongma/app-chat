// 家长绑定 H5 全局配置：OA 后端地址 + 是否走本地 mock。
// 值优先读 Vite 环境变量（.env / .env.production / CI 注入）：
//   VITE_USE_MOCK        "true" 时走本地 mock（默认 false，真实 OA 后端）
//   VITE_OA_BASE_URL     OA 网关地址（留空 = 同域 / 本地 vite proxy）
//   VITE_OA_APPID        公众号 appid（网页授权跳转用）
//   VITE_OA_REDIRECT_URI 网页授权回调地址（默认当前页 URL，可留空）
//   VITE_DEV_SMS_CODE    mock 模式开发验证码（默认 123456）
const env = import.meta.env

export default {
  // OA 网关地址；留空 = 同域（本地走 vite proxy，生产走 nginx 反代）。
  OA_BASE_URL: env.VITE_OA_BASE_URL || '',
  // 公众号 appid（网页授权跳转用；mock 模式可留空）。
  OA_APPID: env.VITE_OA_APPID || '',
  // 网页授权回调地址（留空则用 location.origin + location.pathname）。
  OA_REDIRECT_URI: env.VITE_OA_REDIRECT_URI || '',
  // 是否走本地 mock（默认 false，真实 OA 后端；本地 mock 需 VITE_USE_MOCK=true）。
  USE_MOCK: env.VITE_USE_MOCK === 'true',
  // mock 模式下「发送验证码」后可用的开发验证码。
  DEV_SMS_CODE: env.VITE_DEV_SMS_CODE || '123456',
}
