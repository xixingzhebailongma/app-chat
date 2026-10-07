import { defineConfig, loadEnv } from 'vite'
import vue from '@vitejs/plugin-vue'

// 本地 dev 把 /api 代理到 wechat-gateway（规避跨域）；生产由 nginx 同域反代。
export default defineConfig(({ command, mode }) => {
  // 与 src/config.js 的 import.meta.env 同源：都读 .env* 文件里的 VITE_USE_MOCK，
  // 不读 process.env，避免「构建时判定 false、运行时却是 true」的错配。
  const env = loadEnv(mode, process.cwd(), 'VITE_')
  const useMock = env.VITE_USE_MOCK === 'true'
  if (command === 'build' && useMock) {
    throw new Error(
      '[parent-h5] 生产构建禁止 USE_MOCK=true（当前 VITE_USE_MOCK=true）。' +
      '请设置 VITE_USE_MOCK=false 或移除该变量。'
    )
  }
  return {
    plugins: [vue()],
    server: {
      host: true,
      port: 5174,
      proxy: {
        '/api': {
          target: 'http://127.0.0.1:8080',
          changeOrigin: true,
        },
      },
    },
  }
})
