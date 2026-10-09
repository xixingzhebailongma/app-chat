# wechat-gateway

微信小程序接入的接入层网关（C++ Drogon）。项目总设计文档在仓库根目录
`../CLAUDE.md`；本 README 作为仓库内入口。

- 机器可读契约：[`openapi/miniapp.yaml`](openapi/miniapp.yaml)（对外，硬）+ [`openapi/internal.yaml`](openapi/internal.yaml)（内部，软）

> 交接文档（as-built 对接清单、接口契约清单、联调自测清单等）单独交付，不随本仓库。

## 开发注意

- **路径参数用 `req->getRoutingParameters()` 取，不要用 `getParameter()`**：在
  `registerHandler`（lambda 路由）下，路径参数（如 `/api/.../{id}` 的 `{id}`）落在
  `getRoutingParameters()`（位置向量，按 `{}` 出现顺序）；`getParameter()` 只含
  query/form。参照：`src/controllers/src/AlertController.cpp`（`{id}`）、
  `src/controllers/src/SpaceTypeController.cpp`（`{code}`）。新增带路径参数的接口照此写。

