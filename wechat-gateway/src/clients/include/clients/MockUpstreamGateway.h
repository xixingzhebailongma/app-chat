#pragma once

#include <nlohmann/json.hpp>

#include <string>

#include "clients/IUpstreamGateway.h"

// IUpstreamGateway 的参考 Mock 实现（联调用，独立 target `mock_upstream_gateway`）。
// 不触达真实 go-backend，返回可配置的 canned 响应。用法：构造后当作 IUpstreamGateway
// 注入 AuthService / DeviceService / SceneService / DeviceControlService 即可。
class MockUpstreamGateway : public IUpstreamGateway {
public:
    MockUpstreamGateway() = default;

    // —— 可配置的 canned 数据 ——
    void setLoginUser(const std::string& user_id, const std::string& role,
                      const std::string& username = "",
                      const std::string& name = "") {
        loginUser_ = UpstreamLogin{true, user_id, username, role, name, ""};
    }
    void setPhone(const std::string& phone) { phone_ = phone; }
    void setDevices(const nlohmann::json& devices) { devices_ = devices; }

    void login(const std::string&, const std::string&,
               LoginCallback cb) override {
        cb(loginUser_);
    }

    std::string getUserPhoneSync(const std::string&) override { return phone_; }

    void get(const std::string&, const std::string&, const Headers&,
             RawCallback cb) override {
        UpstreamResponse r;
        r.ok = true;
        r.status = drogon::k200OK;
        r.body = devices_.dump();
        cb(r);
    }

    void post(const std::string&, const std::string&, const Headers&,
              RawCallback cb) override {
        UpstreamResponse r;
        r.ok = true;
        r.status = drogon::k200OK;
        r.body = nlohmann::json{{"ok", true}}.dump();
        cb(r);
    }

private:
    UpstreamLogin loginUser_{true, "u_mock", "mock_user", "teacher", "Mock", ""};
    std::string phone_ = "13800000000";
    nlohmann::json devices_ = nlohmann::json::array();
};
