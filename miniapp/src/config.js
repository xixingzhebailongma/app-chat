// 全局配置：网关地址 + 是否走本地 mock。
// 各值优先读 Vite 环境变量（.env / .env.production / CI 注入），默认走本地 mock：
//   VITE_USE_MOCK           "false" 时切真实网关（默认 true，本地 mock）
//   VITE_BASE_URL           网关地址（默认 http://127.0.0.1:8080；真机/生产用 https 域名）
//   VITE_WS_BASE_URL        WebSocket 地址（留空则从 BASE_URL 派生 http→ws / https→wss）
//   VITE_SUBSCRIBE_TMPL_IDS 订阅消息模板 ID，逗号分隔（≤3 个；为空不拉起订阅授权）
// USE_MOCK = true 时，前端用内置假数据（镜像网关 dev seed + mock go-backend），
// 无需启动后端、无需真机微信授权即可在浏览器/开发者工具里预览整套界面。
const env = import.meta.env

export default {
  // 网关地址；真机调试改为局域网 IP，生产用 https 域名。
  BASE_URL: env.VITE_BASE_URL || 'http://127.0.0.1:8080',
  // WebSocket 地址；留空则从 BASE_URL 自动派生。
  WS_BASE_URL: env.VITE_WS_BASE_URL || '',
  // 是否走本地 mock：显式 VITE_USE_MOCK=false 才连真实网关。
  USE_MOCK: (env.VITE_USE_MOCK ?? 'true') !== 'false',
  // 订阅消息模板 ID（真实模板 ID，需与网关 config 的 wechat_miniapp.templates 一致，
  // 且 ≤3 个）。为空时不拉起微信订阅授权（演示/未配置）。
  SUBSCRIBE_TMPL_IDS: (env.VITE_SUBSCRIBE_TMPL_IDS || '')
    .split(',')
    .map((s) => s.trim())
    .filter(Boolean),
}
