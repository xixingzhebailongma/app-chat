# 交接包 MANIFEST

> ⚠️ **miniapp/dist/ 已从交付包移除，接手后必须用 src 本地 build，禁止使用旧产物。**

日期: 2026-10-03
包名: miniapp-handover-20261003b.zip

## 源码版本 / commit

- miniapp（uni-app 前端）
  commit 6ae55834d7466d5b1200386f7c274c323e7f9eee  branch main
  2026-09-30 09:21:37 +0800  feat: 告警/设备错误码映射 + 告警页空 id 回退 + 订阅模板截断
- wechat-gateway（cpp-bff，C++ Drogon）
  commit 26f2905  branch feat/authz-errorcodes
  2026-10-07  docs: 7.8/7.9 收尾与交接文档更新
  其上 6cbbfd6  feat(oa-notify): 到校通知 real/mock 语义 + 家长可达性 + errcode 落库 + 健康检查
  快照基线 7fefd72  docs: 对齐 /alerts/{id}/handle 的 status 可选契约
- parent-h5（家长绑定 H5）
  commit 91db9329cfb470d24fa1e86a6739974f783383ed  branch main
  2026-09-30 14:40:40 +0800  docs: README 补后端联调说明

> 注：本包为**工作区快照**——三个仓库（wechat-gateway / miniapp / parent-h5）当前 `git status`
> 均含未提交改动（`M`/`??`），随包按**当前工作区内容**打包，非纯 commit 状态。

## 未提交改动清单（miniapp / parent-h5 工作区，2026-10-03）

> 「影响 build」= 编译进 dist 产物；「否」= 仅测试/文档，不影响运行产物。

### miniapp（uni-app 前端，branch main @ 6ae5583）

| 文件 | 状态 | 改动用途 | 影响 build |
|---|---|---|---|
| `src/pages/classroom/classroom.vue` | M | 「今日进出」入口带 `auth_type=face` 跳转 records 页 | 是 |
| `src/pages/records/records.vue` | M | 进出记录页新增「方式筛选」（face/card），`fetchRecords` 追加 `auth_type` 参数 | 是 |
| `src/pages/home/home.vue` | M | 注释更新（N+1 设备聚合技术债说明） | 是（注释级） |
| `src/utils/env.js` | M | `toNum` 增加字符串数字解析（传感器 app 字段兼容字符串） | 是 |
| `src/utils/request.js` | M | 统一错误文案：403 无权限 / 503 功能未上线 | 是 |
| `tests/smoke_mock_api.mjs` | M | 冒烟补「方式筛选 face/card」断言 | 否 |
| `tests/unit_env.mjs` | ?? | env.js 单元测试（新增） | 否 |
| `docs/n+1-设备聚合-技术债.md` | ?? | 新增技术债文档 | 否 |

### parent-h5（家长绑定 H5，branch main @ 91db932）

| 文件 | 状态 | 改动用途 | 影响 build |
|---|---|---|---|
| `src/views/Bind.vue` | M | 绑定页加「该学号已绑定，无需重复绑定」校验 | 是 |

## 构建命令与结果（2026-10-02 实测，node v26 / npm 11）

- miniapp:  npm run build:mp-weixin   ->  DONE Build complete（退出码 0）
- miniapp:  npm run build:h5          ->  DONE Build complete（退出码 0）
- 唯一告警：Sass legacy-js-api 弃用警告，不影响产物。
- 说明：构建时临时拿开本地 .env，产物用干净的 USE_MOCK=true 默认值，不含内网 IP。

## 产物路径（随包）

- ~~miniapp/dist/build/mp-weixin/~~  **已移除**（2026-10-03b）：不携带前端 dist 产物，接手后本地 build。
- ~~miniapp/dist/build/h5/~~      **已移除**（同上）

## SHA256（产物目录树）

- 本包不再携带 `miniapp/dist/`，无产物 SHA256。

> 注：本 MANIFEST 随包打入 zip，故不含 zip 自身 sha256；zip 的 sha256 由交付方在回报中单独给出。

---

## 修订记录（2026-10-07 交接前清理 f99b7fd）

- 本地提交 `f99b7fd` chore(handover): 交接前清理（未 push、无 remote；本地分支 `main`）：
  - 清 5 处过期「TODO: 实现 PG 仓库」注释（`src/db/include/db/` 下 5 个头文件，PG 实现早已存在并接线）。
  - 合并 mock-go-backend：**保留严格版 `scripts/mock-go-backend.py`，删除宽松版 `scripts/dev/mock-go-backend.py`**；
    `scripts/dev/e2e-77-verify.py` 登录密码由 `secret` 改为 `admin123` 对齐严格版——这是**被测对象替换**
    （e2e 原本写给宽松版、后者不校验密码），**不是断言放宽**。
  - 登录页（`miniapp/src/pages/login/login.vue`）演示模式去掉明文密码，提示改为「演示模式 · 直接登录即可」，
    并预填管理员账号。
- 鉴权 smoke 脚本正名：实际文件名是 `wechat-gateway/tests/smoke_auth.sh`（3 断言：过期 JWT→401、教师跨空间→403 × 2；
  位于 git 忽略的 `tests/`，不进仓库），与仓库内 `scripts/demo-authz.sh`（鉴权演示脚本，无 PASS/FAIL 计数）是
  两个不同脚本，本处不再混称。
- `.vscode/` 与 `api-tests/` 由 git 跟踪**移出仓库**（`git rm --cached` + `.gitignore` 忽略），本地文件保留；
  这两项目不再随仓库交付。

## 修订记录（2026-10-07 收口提交）

- wechat-gateway 源码：代码已 squash 导入本仓库，根提交 `1423a3a` 含全部 7.8 收口改动（未 push，本地分支 `main`）。
- 验证全绿（2026-10-07 实测）：`cmake --build` 100%；`ctest` 16/16（pg_* 无 PG 连接按设计跳过）；
  `mock e2e`（`scripts/dev/e2e-77-verify.py`）23/23；`scripts/smoke_test.sh` 15/0；`tests/smoke_auth.sh` 3/0。
- 收口过程两处修正：
  - `.gitignore` 补 `/uploads/`（运行时上传目录，避免漏进 git status）。
  - `scripts/dev/e2e-77-verify.py` 段 2 断言过时（JwtFilter 实时角色覆盖已提交）→ 改用独立演示用户
    `u_role_demo` 验证「实时覆盖」，未改 JwtFilter 缓存行为；`docs/端到端验证报告.md` 段 2 同步更新。
- 收口后新增文档：`剩余工作待办清单.md`（按负责人/阻塞/优先级分类）。

## 修订记录（2026-10-03 交接定稿）

- `GO-接手清单.md`：最前面新增「给 Go 后端的 10 条纠偏」+「数据/调用方向图」；§1 新增「常见误寻的 /internal/*（不存在）」表。
- `docs/7.7-as-built.md`：新增「7.8/7.9 落地状态」专节；修正「角色变更须重新登录」→ 已闭环（JwtFilter 实时覆盖 user_roles，60s TTL）。
- `docs/小程序接入-as-built-对接清单.md`：§1.2/§1.6/§1.7 补落地状态 + 角色实时生效更正。
- `openapi/internal.yaml`（三份拷贝一致）：`/internal/user-roles/sync` 补 400 + `roles` 删除语义。
- `testcases.md`：新增「三A. 后端关键验收用例（7 条硬性）」。
- `cpp-bff/`：整仓重同步自 `wechat-gateway`（排除 build/third_party/.git/Testing/uploads/operation_logs.jsonl）。
- 本轮**移除** `miniapp/dist/` 产物（源码含未提交改动，避免旧产物与源码不一致）；新增「未提交改动清单」节；接手后一律用 src 本地 build。

## 修订记录（2026-10-02 收尾）

- `README-交接.md` §1：修正「C++ 网关不随本交接包」的矛盾表述 → 明确随包附于 `cpp-bff/`，并说明 `third_party/` 需 `scripts/setup-deps.sh` 重建（sudo + 联网 + 从源码编译 Drogon）。
- `已知问题.md`：新增「2.0 外部依赖责任表」，明确三项外部依赖（凭证 / 边侧回灌 / 设备真实性）阻塞，非本包代码未完成。
- 本次仅改文档，源码与 `dist/` 产物未改动。原 MANIFEST 中 dist 的 SHA256 计算方式未标注（是否含 tar 头、排序方式），本次无法一致复算，故保留原值。
