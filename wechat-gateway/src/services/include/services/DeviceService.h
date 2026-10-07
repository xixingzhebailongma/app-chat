#pragma once

#include <drogon/HttpTypes.h>
#include <nlohmann/json.hpp>

#include <functional>
#include <memory>
#include <string>

#include "clients/GoBackendClient.h"
#include "db/UserSpacesRepository.h"
#include "utils/ErrorBody.h"

// 返回给控制器的结果，用于 GET /api/miniapp/devices。`body` 承载本地生成的
// {"error":{...}} 结构（空间权限被拒）或原样透传的 go-backend 响应主体。
struct DeviceResult {
    drogon::HttpStatusCode status = drogon::k500InternalServerError;
    std::string body;
    drogon::ContentType contentType = drogon::CT_APPLICATION_JSON;

    static DeviceResult error(drogon::HttpStatusCode s,
                              const std::string& msg) {
        DeviceResult r;
        r.status = s;
        r.body = http_util::errorBodyForStatus(static_cast<int>(s), msg).dump();
        return r;
    }

    // 带显式机器可读字符串代码的错误（如 "GO_BACKEND_UNAVAILABLE"），
    // 对齐 DeviceControlResult::errorWithCode。
    static DeviceResult errorWithCode(drogon::HttpStatusCode s,
                                      const std::string& code,
                                      const std::string& msg) {
        DeviceResult r;
        r.status = s;
        r.body = http_util::errorBody(code, msg).dump();
        return r;
    }
};

// GET /api/miniapp/devices（设计文档 八 控制指令转发 优先级2）：网关在本地
// 执行空间边界（管理员，或已绑定该空间的教师），然后将按空间过滤的设备
// 列表转发给 go-backend 并原样返回响应。
class DeviceService {
public:
    using Callback = std::function<void(const DeviceResult&)>;

    DeviceService(std::shared_ptr<GoBackendClient> goBackend,
                  std::shared_ptr<UserSpacesRepository> spaces);

    void listDevices(const std::string& userId, const std::string& role,
                     const std::string& jwt, const std::string& spaceId,
                     Callback cb);

private:
    std::shared_ptr<GoBackendClient> goBackend_;
    std::shared_ptr<UserSpacesRepository> spaces_;
};
