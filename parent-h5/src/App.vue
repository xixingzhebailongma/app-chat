<template>
  <Bind />
</template>

<script>
import Bind from './views/Bind.vue'
export default { name: 'App', components: { Bind } }
</script>

<style>
/* 设计 token：与 miniapp/App.vue 同源（Element Plus 风格，主色 #409eff），单位改 px 适配 web。 */
:root {
  --primary: #409eff;
  --primary-active: #337ecc;
  --primary-soft: #ecf5ff;
  --primary-border: #d9ecff;
  --text-1: #303133;
  --text-2: #606266;
  --text-3: #909399;
  --text-4: #c0c4cc;
  --border: #dcdfe6;
  --border-light: #e4e7ed;
  --border-lighter: #ebeef5;
  --bg: #f5f7fa;
  --success: #67c23a;
  --success-bg: #f0f9eb;
  --warning: #e6a23c;
  --warning-bg: #fdf6ec;
  --danger: #f56c6c;
  --danger-bg: #fef0f0;
  --radius: 8px;
  --radius-lg: 12px;
  --shadow: 0 1px 4px rgba(0, 0, 0, 0.04);
}
* { box-sizing: border-box; }
html, body { margin: 0; padding: 0; }
body {
  background: var(--bg);
  color: var(--text-1);
  font-size: 15px;
  line-height: 1.5;
  font-family: -apple-system, BlinkMacSystemFont, "Helvetica Neue", "PingFang SC",
    "Hiragino Sans GB", "Microsoft YaHei", sans-serif;
  -webkit-font-smoothing: antialiased;
}
#app { min-height: 100vh; }

.card {
  background: #fff;
  border: 1px solid var(--border-light);
  border-radius: var(--radius-lg);
  box-shadow: var(--shadow);
}

.btn {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 100%;
  height: 46px;
  border: 1px solid transparent;
  border-radius: 8px;
  font-size: 15px;
  cursor: pointer;
  transition: background 0.2s;
  padding: 0 16px;
  outline: none;
}
.btn-primary { background: var(--primary); border-color: var(--primary); color: #fff; }
.btn-primary:active { background: var(--primary-active); }
.btn-primary:disabled { opacity: 0.5; cursor: not-allowed; }
.btn-ghost { background: var(--primary-soft); border-color: var(--primary-soft); color: var(--primary); }
.btn-ghost:disabled { opacity: 0.5; cursor: not-allowed; }
.btn-plain { background: #fff; color: var(--text-2); border-color: var(--border); }
</style>
