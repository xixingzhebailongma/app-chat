#pragma once

#include <drogon/WebSocketController.h>
#include <drogon/WebSocketClient.h>

#include <memory>
#include <string>

#include "db/UserSpacesRepository.h"

// 边侧 go-backend /ws 的 WebSocket 代理（设计文档 7.4② 设备状态同步）：
// 小程序连网关 /ws?token=<jwt>&space_id=<sid>，网关校验 JWT + 空间边界后，
// 透明转发到边侧 /ws，并把 device_update / cmd_response / ping 原样回推。
class WsProxyController
    : public drogon::WebSocketController<WsProxyController> {
public:
    void handleNewMessage(const drogon::WebSocketConnectionPtr& conn,
                          std::string&& message,
                          const drogon::WebSocketMessageType& type) override;
    void handleNewConnection(const drogon::HttpRequestPtr& req,
                             const drogon::WebSocketConnectionPtr& conn) override;
    void handleConnectionClosed(
        const drogon::WebSocketConnectionPtr& conn) override;

    WS_PATH_LIST_BEGIN
    WS_PATH_ADD("/ws");
    WS_PATH_LIST_END

    // 启动时注入依赖（main.cpp 调用一次）。复用本项目的静态注入模式
    // （见 JwtUtil::setSecret / InternalTokenFilter::setToken）。
    static void configure(std::string goBackendBaseUrl,
                          std::shared_ptr<UserSpacesRepository> spaces);

private:
    // 每个小程序连接对应的边侧客户端，存放在连接上下文里。
    struct ProxyContext {
        drogon::WebSocketClientPtr edge;
    };

    std::string edgeWsUrl() const;

    static std::string goBackendBaseUrl_;
    static std::shared_ptr<UserSpacesRepository> spaces_;
};
