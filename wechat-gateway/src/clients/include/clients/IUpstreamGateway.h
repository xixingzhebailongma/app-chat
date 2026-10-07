#pragma once

#include <drogon/HttpTypes.h>

#include <fstream>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

#include <nlohmann/json.hpp>
#include <trantor/utils/Logger.h>

// 上游边侧 go-backend 的配置（config/upstream.json）：路径 + 字段名可配。
// 默认值 = GoBackendClient 现硬编码值（等价于现状）。重启生效，不支持热加载。
struct UpstreamConfig {
    std::string loginPath = "/auth/login";
    std::string loginReqUsername = "username";
    std::string loginReqPassword = "password";
    std::string loginUserKey = "user";
    std::string loginUserId = "user_id";
    std::string loginUsername = "username";
    std::string loginName = "name";
    std::string loginRole = "role";

    std::string userPhonePath = "/users/{user_id}";
    std::string userPhoneHeader = "X-Internal-Token";
    std::string userPhoneKey = "phone";

    std::string devicesListPath = "/devices";
    std::string devicesSpaceIdParam = "space_id";
    std::string devicesControlPath = "/devices/{device_type}/{device_id}/cmd";
    std::string controlCommandKey = "command";

    std::string devIdKey = "device_id";
    std::string devTypeKey = "device_type";
    std::string devLabelKey = "label";
    std::string devZbRoleKey = "zb_role";  // 保留字段：真实边侧无 zb_role（仅 zb_type），生产规则留空走 label 兜底

    std::string wsPath = "/ws";

    static UpstreamConfig& instance() {
        static UpstreamConfig c;
        return c;
    }

    // 文件缺失 -> 保留内置默认；非法 JSON -> 抛 std::runtime_error（调用方 LOG_FATAL）。
    void loadFromFile(const std::string& path) {
        std::ifstream in(path);
        if (!in.good()) {
            LOG_WARN << "upstream.json missing (" << path
                     << "); using built-in defaults";
            return;
        }
        nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
        if (j.is_discarded() || !j.is_object()) {
            throw std::runtime_error("upstream.json is not a valid JSON object");
        }
        if (j.contains("login") && j["login"].is_object()) {
            const auto& l = j["login"];
            loginPath = l.value("path", loginPath);
            if (l.contains("request")) {
                loginReqUsername = l["request"].value("username", loginReqUsername);
                loginReqPassword = l["request"].value("password", loginReqPassword);
            }
            if (l.contains("response")) {
                loginUserKey = l["response"].value("user_key", loginUserKey);
                loginUserId = l["response"].value("user_id", loginUserId);
                loginUsername = l["response"].value("username", loginUsername);
                loginName = l["response"].value("name", loginName);
                loginRole = l["response"].value("role", loginRole);
            }
        }
        if (j.contains("user_phone") && j["user_phone"].is_object()) {
            const auto& u = j["user_phone"];
            userPhonePath = u.value("path", userPhonePath);
            userPhoneHeader = u.value("internal_token_header", userPhoneHeader);
            if (u.contains("response")) {
                userPhoneKey = u["response"].value("phone", userPhoneKey);
            }
        }
        if (j.contains("devices") && j["devices"].is_object()) {
            const auto& d = j["devices"];
            devicesListPath = d.value("list_path", devicesListPath);
            devicesSpaceIdParam = d.value("list_query_space_id", devicesSpaceIdParam);
            devicesControlPath = d.value("control_path", devicesControlPath);
            if (d.contains("control_request")) {
                controlCommandKey = d["control_request"].value("command", controlCommandKey);
            }
            if (d.contains("device_fields")) {
                devIdKey = d["device_fields"].value("device_id", devIdKey);
                devTypeKey = d["device_fields"].value("device_type", devTypeKey);
                devLabelKey = d["device_fields"].value("label", devLabelKey);
                devZbRoleKey = d["device_fields"].value("zb_role", devZbRoleKey);
            }
        }
        wsPath = j.value("ws_path", wsPath);
    }

    // 路径模板替换：把 {key} 换成 vars[key]。
    static std::string fill(std::string tmpl,
                            const std::map<std::string, std::string>& vars) {
        for (const auto& [k, v] : vars) {
            const std::string token = "{" + k + "}";
            std::size_t pos = 0;
            while ((pos = tmpl.find(token, pos)) != std::string::npos) {
                tmpl.replace(pos, token.size(), v);
                pos += v.size();
            }
        }
        return tmpl;
    }
};

// 通用 go-backend 请求的原始透传结果。
struct UpstreamResponse {
    bool ok = false;  // 仅传输失败（不可达）时为 false
    drogon::HttpStatusCode status = drogon::k500InternalServerError;
    std::string body;
    drogon::ContentType contentType = drogon::CT_APPLICATION_JSON;
    std::string errmsg;
};

// go-backend 的 POST /api/auth/login 的结果。
struct UpstreamLogin {
    bool ok = false;
    std::string user_id;
    std::string username;
    std::string role;
    std::string name;
    std::string errmsg;
};

// 上游网关抽象：把网关对边侧 go-backend 的依赖收敛到一个接口，
// 联调方可用 Mock 实现替换真实客户端（config/upstream.json 控制路径/字段名）。
class IUpstreamGateway {
public:
    using Headers = std::unordered_map<std::string, std::string>;
    using LoginCallback = std::function<void(const UpstreamLogin&)>;
    using RawCallback = std::function<void(const UpstreamResponse&)>;

    virtual ~IUpstreamGateway() = default;

    virtual void login(const std::string& username, const std::string& password,
                       LoginCallback cb) = 0;

    // 查手机号（短信兜底）。同步阻塞 HTTP——**刻意保留同步**：通知分发在后台线程
    // 同步串行，短信兜底需在渠道 send 前拿到 target；绝不能在 Drogon 事件循环线程调用。
    virtual std::string getUserPhoneSync(const std::string& user_id) = 0;

    virtual void get(const std::string& path, const std::string& query,
                     const Headers& headers, RawCallback cb) = 0;

    virtual void post(const std::string& path, const std::string& body,
                      const Headers& headers, RawCallback cb) = 0;

    static Headers bearer(const std::string& jwt) {
        return {{"Authorization", "Bearer " + jwt}};
    }
};
