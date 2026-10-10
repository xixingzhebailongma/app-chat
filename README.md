###### 项目根目录

├── wechat-gateway/   # C++ Drogon 后端网关（8080），鉴权/代理/通知推送；          


├── miniapp/          # 教师/管理员小程序前端（uni-app，Vue3+Vite）； 				


├── parent-h5/        # 家长「绑定孩子」H5（Vue3+Vite）；							


├── api-tests/        # 边侧 go-backend 接口审计/测试脚本；						


├── .gitignore        # 忽略规则；文件数：—

└── .vscode/          # 编辑器配置（只有 settings.json）；																







首次构建（C++ 网关）：仓库不含 third_party/（Drogon 依赖，已 gitignore）。首次构建前先跑 wechat-gateway/scripts/setup-deps.sh（需 sudo、联网，从源码编译 Drogon + trantor，耗时较长），然后 cmake -S . -B build -DCMAKE_PREFIX_PATH=$PWD/third_party/install && cmake --build build -j。nlohmann/json 由 CMake 自动拉取；libpq-dev 可选但生产必需（缺它则无 PG 后端、APP_ENV=prod 拒绝启动

接手需知：占位 / 临时 / 测试态清单

1. 网关指向的 Go 后端地址（当前是假地址）

- go_backend.base_url 默认 http://127.0.0.1:8081 —— 本机回环，当前没有服务监听这里；可用环境变量 GO_BACKEND_URL 覆盖。
- go_backend.api_prefix 默认 /api —— 该字段没有环境变量覆盖，只能改 config/config.json。
- internal_token 默认 dev-internal-token（占位）；可用环境变量 INTERNAL_TOKEN 覆盖。

## 2. 微信 / 短信凭证（全是占位，需要真实值）

- **小程序 appid / secret**
  - 当前占位值：`dev-miniapp-appid` / `dev-miniapp-secret`
  - 真实值环境变量：`WECHAT_APPID` / `WECHAT_SECRET`

- **公众号 appid / secret**
  - 当前占位值：`dev-oa-appid` / `dev-oa-secret`
  - 真实值环境变量：`WECHAT_OA_APPID` / `WECHAT_OA_SECRET`

- **订阅消息模板 ID**
  - 当前占位值：`tmpl_001` / `tmpl_002` / `tmpl_003`
  - 真实值环境变量：`MINIAPP_TMPL_*`（4 个事件各一个）

- **到校通知模板 ID**
  - 当前占位值：`tmpl_arrival`
  - 真实值环境变量：`WECHAT_OA_TMPL_ARRIVAL`

- **短信 access_key / secret**
  - 当前占位值：字面 `${SMS_ACCESS_KEY}` / `${SMS_SECRET}`
  - 真实值环境变量：`SMS_ACCESS_KEY` / `SMS_SECRET`

- **短信 sdk_app_id**
  - 当前占位值：`1400000000`（假值）
  - 真实值环境变量：—

- **短信模板 code**
  - 当前占位值：`SMS_123456` ~ `SMS_123459`、`SMS_OA_CODE`
  - 真实值环境变量：—

- **JWT secret**
  - 当前占位值：`dev-shared-jwt-secret`
  - 真实值环境变量：`JWT_SECRET`

前端：

- miniapp/src/manifest.json：小程序 appid 为空、urlCheck=false（关掉了域名校验）。
- miniapp/src/config.js：SUBSCRIBE_TMPL_IDS 为空（不拉订阅授权弹窗）。
- parent-h5/src/config.js：OA_APPID、OA_REDIRECT_URI 为空。

部署：

- wechat-gateway/deploy/k8s.yaml：Secret 里所有值都是 CHANGE_ME 占位。

3. 演示账号与假数据（只存在于 mock / 开发环境）

- 小程序登录页预填默认管理员：admin / admin123。
- 三组演示账号：admin / admin123（管理员）、li / 123456（教师）、wang / 123456（教师）。
- 家长 H5 mock 验证码固定 123456。
- sql/seed_dev.sql 整份是开发演示数据（生产不执行）。
- scripts/mock-go-backend.py 是 go-backend 的替身（非生产依赖）。
- miniapp/src/api/mock.js、parent-h5/src/mock.js 是内置假数据。

4. 测试态 → 生产态开关（现在是测试态）

- 通知渠道 mode 默认全是 mock（wechat_miniapp / sms / oa，dry-run 不真发）。
- APP_ENV 默认 dev。
- postgres.conninfo 默认空 —— 未配 PostgreSQL。
- redis.host 默认空 —— 未配 Redis。
- 小程序 USE_MOCK 默认 true、BASE_URL 默认 http://127.0.0.1:8080。
- 钉钉 / 企业微信渠道 enabled=false。

5. 一句话提醒

- APP_ENV=prod 时，上面这些占位值会让网关拒绝启动（已内置快速失败），所以凭证不换成真实值、生产环境起不来。





VITE_USE_MOCK=false
VITE_BASE_URL=https://<网关域名>
2 +# 预览：切回 mock，浏览器直接看整套界面（无需后端/微信凭证）
3 +VITE_USE_MOCK=true