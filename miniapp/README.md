# 智慧校园数字基座 · 教师端小程序（uni-app）

独立于 `wechat-gateway`（纯 C++ 后端）的前端项目。用 **uni-app（Vue3 + Vite）** 编写，
设计语言对照「数字基座 · 云端管理平台」（Element Plus 风格，主色 `#409eff`），
核心目标是让老师**一眼看清该关注的信息**：设备在线/离线统计、待处理告警、空间边界。

## 页面

| 页面 | 路径 | 说明 |
| --- | --- | --- |
| 登录 | `pages/login/login` | mock 演示登录 / 真实微信登录 + 账号绑定 |
| 首页总览 | `pages/home/home` | 统计卡片、待处理告警预览、快捷入口 |
| 设备与空间 | `pages/devices/devices` | 按空间查看设备、下发控制指令 |
| 告警 | `pages/alerts/alerts` | 状态筛选、管理员处理告警 |
| 我的 | `pages/profile/profile` | 身份信息、订阅消息开关、退出 |

## 快速开始

```bash
npm install

# 浏览器预览（H5，默认走本地 mock，无需后端/微信授权）
npm run dev:h5          # 或 npm run build:h5 后静态托管 dist/build/h5

# 微信小程序（产物在 dist/dev/mp-weixin，用微信开发者工具导入该目录）
npm run dev:mp-weixin
```

> 默认 `VITE_USE_MOCK = true`：内置假数据镜像网关的 dev seed + mock-go-backend，
> 打开就能看到完整界面（管理员/教师两种视角）。

## 接入真实网关

通过 `.env` 配置（变量经 `src/config.js` 读取 `import.meta.env`）：

```bash
VITE_USE_MOCK=false                  # 接真实网关必须显式置 false（默认 true 走本地 mock）
VITE_BASE_URL=http://127.0.0.1:8080  # 网关地址；真机调试改为局域网 IP
```

变量清单见 `.env.example`（`VITE_BASE_URL` / `VITE_WS_BASE_URL` / `VITE_USE_MOCK` / `VITE_SUBSCRIBE_TMPL_IDS`）。

需先启动 `wechat-gateway`（监听 8080）与 `scripts/mock-go-backend.py`（监听 8081）。

## 对接的后端接口

全部位于 `wechat-gateway`，带 JWT（`Authorization: Bearer <token>`）：

- `POST /api/miniapp/login` — `wx.login` code 换 token / `need_bind`
- `POST /api/miniapp/bind` — 账号密码绑定换 token
- `GET  /api/miniapp/me`
- `GET  /api/miniapp/spaces` — 老师只见自己被授权的空间
- `GET  /api/miniapp/devices?space_id=`
- `GET  /api/miniapp/alerts` — 老师视角会脱敏 `operator_id/remark`
- `POST /api/miniapp/alerts/{id}/handle` — 仅管理员
- `POST /api/miniapp/device/control` — 指令白名单 `on/off/toggle`；门禁白名单 `door_devices` 设备下发 `off` 需 `confirm=true`（否则 428 `CONFIRM_REQUIRED`），`toggle` 直接 400
- `POST /api/miniapp/subscribe`

完整接口契约（字段/枚举/错误码/WS）见 `wechat-gateway/docs/前端miniapp-交接接口.md`，机器契约见 `wechat-gateway/openapi/miniapp.yaml`。

## 目录结构

```
src/
  config.js            # 读取 VITE_* 环境变量（BASE_URL / USE_MOCK 开关）
  pages/               # 5 个页面
  components/          # AppTabBar / StatCard / StatusTag / EmptyState
  api/                 # index.js（接口）+ mock.js（假数据）
  utils/               # request（JWT 封装）/ auth（会话持久化）/ format（中文映射）
  store/index.js       # 轻量会话 + 未处理告警红点
  App.vue              # 全局设计系统（CSS 变量）
```

## 备注

- 告警接口返回 `created_at`（后端 `Alert` 已带该字段），列表/详情时间戳为真实数据。
