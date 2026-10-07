# 家长绑定 H5（parent-h5）

学校公众号菜单挂载的「绑定孩子」页面：家长通过网页授权绑定孩子学号，绑定后孩子到校通过公众号模板消息通知家长。

## 定位与范围

**只做前端页面**。不包含：后端花名册校验（`student_parents`）、真实公众号发送、逐家长幂等/去重、H5 部署上线——这些在 `wechat-gateway` 后端 / 部署侧，本仓库不涉及。

## 技术栈

Vite + Vue3，单页多状态（无需 vue-router）：`授权 → 表单 / 已绑定列表`（多孩子，可删可加）。

## 目录说明

```
src/
  config.js        # 环境变量 + USE_MOCK 开关 + OA 配置
  api.js           # authorize / sendCode / confirm / me / unbind（USE_MOCK 分流 + 错误体解析）
  mock.js          # 本地假实现（无公众号凭证也能跑通；多孩子用 localStorage 持久化）
  App.vue          # 全局设计 token（复用 miniapp 的 #409eff 体系）
  views/Bind.vue   # 单页多状态：授权 → 表单 / 已绑定列表（多孩子，可删可加）
```

## 环境变量

真值不入库（写入 `.env` / `.env.production`，参考 `.env.example`）：

| 变量 | 说明 | 默认 |
|---|---|---|
| `VITE_USE_MOCK` | `false` 走真实 OA 后端 | `true` |
| `VITE_OA_BASE_URL` | OA 网关地址（留空=同域/走 vite proxy） | `''` |
| `VITE_OA_APPID` | 公众号 appid（网页授权跳转） | `''` |
| `VITE_OA_REDIRECT_URI` | 网页授权回调地址（留空=当前页 URL） | `''` |
| `VITE_DEV_SMS_CODE` | mock 模式开发验证码 | `123456` |

## 本地开发

### mock 模式（无外部凭证，推荐先跑通）

```bash
VITE_USE_MOCK=true npm run dev
```

跳过真实 OAuth，`sendCode` 直接成功，验证码固定 `123456`，走通「授权 → 表单 → 成功/失败/换绑」全流程。

> 说明：`VITE_USE_MOCK=true` 走的是**前端 mock**（`src/mock.js`），不触后端，所以一键启动
> 只需 `npm run dev`、无需起网关。后端契约的验证另走 `wechat-gateway`：

### 后端联调（可选）

```bash
# 网关 InMemory 模式（dev：内存仓库 + mock 渠道，无需 PG/Redis），监听 8080
cd ../wechat-gateway && ./build/wechat-gateway
```

- 后端单测：`cd ../wechat-gateway && ./build/oa_bind_test && ./build/oa_notify_test`
- 直调限制：`authorize`（code 换 `openid_token`）走微信 `/sns/oauth2/access_token`，**无 mock 路径**，
  需真实公众号 code；`send-code`/`confirm`/`me`/`unbind` 可直调，但 mock 短信验证码不对外回显，
  `confirm` 无法纯本地闭环。契约见「后端契约」表。

### 真实模式

```bash
VITE_USE_MOCK=false VITE_OA_APPID=<appid> npm run dev
```

前提：公众号 appid/secret（后端配置）+ 公众号后台「网页授权域名」+ 网关可达 + HTTPS。本地 dev 用 `/api` 代理到网关（见 `vite.config.js`）。

## 构建

```bash
npm run build
```

产物在 `dist/`，由部署侧托管（挂到学校公众号菜单）。

## 后端契约

| 接口 | 说明 |
|---|---|
| `GET /api/oa/bind/authorize?code=` | code 换 `{openid, openid_token}` |
| `POST /api/oa/bind/send-code` | `{phone}` → `{sent:true}` |
| `POST /api/oa/bind/confirm` | `{openid_token, student_no, phone, sms_code}` → 绑定结果（加一个，多孩子追加） |
| `GET /api/oa/bind/me?openid_token=` | 查列表：未绑定 `{bound:false, bindings:[]}`；已绑定 `{bound:true, bindings:[{student_no, student_name, phone, bound_at}, …]}` |
| `POST /api/oa/bind/unbind` | `{openid_token, student_no}` → `{unbound:true}`（删一个） |

> 契约冻结（2026-09）：家长端为**多孩子**模型，`me` 返回 `bindings[]` 数组、`unbind` 需带 `student_no`；「换绑」不作为独立前端概念（= 删 + 加）。
> 共通字段：`bound_at`（ISO 时间）；`openid_token` 为短时 token（每次进 H5 重新 OAuth 换取）；`student_name` 由后端 join 花名册，缺失时省略 → 前端回退只显示学号。

> 注意：7.8 设计文档 §6.2 里的 `/api/oa/bind/oauth`、`/api/oa/bind/verify` 是旧名，**真实路由是 `/authorize` 与 `/confirm`**。

## 已知限制

- `GET /api/oa/bind/me` 与 `POST /api/oa/bind/unbind` 契约已冻结，后端已实现（`wechat-gateway` `OaBindService`，含 InMemory/Pg 两套仓库）。
- `openid_token` 为短时 JWT（约 5 分钟）：`confirm`/`me`/`unbind` 返回 401 时前端自动引导重新授权。
- `student_name` 依赖后端 join 花名册；缺失时前端回退只显示学号。

## 验证方式

1. mock 走通全流程：`VITE_USE_MOCK=true npm run dev` → 绑 A 见 1 张卡 → 绑 B 见 2 张卡 → 删 A 只剩 B → reload 仍是 B。
2. 构建通过：`npm run build`。
