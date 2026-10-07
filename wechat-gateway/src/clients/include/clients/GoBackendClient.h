#pragma once

#include <drogon/HttpTypes.h>

#include <string>

#include "clients/IUpstreamGateway.h"

// 向后兼容别名：历史代码用 GoBackendLogin / GoBackendResponse 名字。
using GoBackendLogin = UpstreamLogin;
using GoBackendResponse = UpstreamResponse;

// go-backend 客户端：实现 IUpstreamGateway。登录/查手机号的路径与字段名经
// config/upstream.json 配置（默认 = 现硬编码值）；设备列表/控制走通用 get/post，
// 路径由调用方经 UpstreamConfig 拼出。
class GoBackendClient : public IUpstreamGateway {
public:
    explicit GoBackendClient(std::string baseUrl);

    // 内部令牌（与 go-backend 共享的 X-Internal-Token），供 getUserPhoneSync 用。
    void setInternalToken(std::string token);

    // 边侧 API 路径前缀（默认 "/api"，来自 config.json 的 go_backend.api_prefix）。
    void setApiPrefix(std::string prefix);

    void login(const std::string& username, const std::string& password,
               LoginCallback cb) override;

    // 同步阻塞 HTTP——绝不能在 Drogon 事件循环线程上调用（见 IUpstreamGateway 注释）。
    std::string getUserPhoneSync(const std::string& user_id) override;

    void get(const std::string& path, const std::string& query,
             const Headers& headers, RawCallback cb) override;

    void post(const std::string& path, const std::string& body,
              const Headers& headers, RawCallback cb) override;

private:
    void request(drogon::HttpMethod method, const std::string& path,
                 const std::string& query, const std::string& body,
                 const Headers& headers, RawCallback cb);

    std::string baseUrl_;
    std::string internalToken_;
    std::string apiPrefix_ = "/api";
};
