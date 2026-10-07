#include "controllers/WsProxyController.h"

#include <trantor/utils/Logger.h>

#include <utility>

#include "utils/Authz.h"
#include "utils/JwtUtil.h"

std::string WsProxyController::goBackendBaseUrl_;
std::shared_ptr<UserSpacesRepository> WsProxyController::spaces_;

void WsProxyController::configure(
    std::string goBackendBaseUrl,
    std::shared_ptr<UserSpacesRepository> spaces) {
    goBackendBaseUrl_ = std::move(goBackendBaseUrl);
    spaces_ = std::move(spaces);
}

// go_backend.base_url（http/https）→ ws/wss，供 WebSocketClient 连接。
std::string WsProxyController::edgeWsUrl() const {
    std::string url = goBackendBaseUrl_;
    while (!url.empty() && url.back() == '/') {
        url.pop_back();
    }
    if (url.rfind("https://", 0) == 0) {
        return "wss://" + url.substr(8);
    }
    if (url.rfind("http://", 0) == 0) {
        return "ws://" + url.substr(7);
    }
    return url;  // 已形如 ws:// 或 wss://
}

void WsProxyController::handleNewConnection(
    const drogon::HttpRequestPtr& req,
    const drogon::WebSocketConnectionPtr& conn) {
    const std::string token = req->getParameter("token");
    const std::string spaceId = req->getParameter("space_id");

    // 1. JWT 校验（与 JwtFilter 同源：共享 go-backend 密钥）。
    const auto claims = JwtUtil::verifyAccessToken(token);
    if (!claims) {
        LOG_WARN << "ws proxy: reject connection (bad or missing token)";
        conn->forceClose();
        return;
    }

    // 2. 空间边界：admin 可任意；teacher 必须指定且已绑定 space_id。
    if (!spaces_ ||
        !authz::canAccessSpace(claims->role, claims->user_id, spaceId,
                               *spaces_)) {
        LOG_WARN << "ws proxy: reject connection (no space permission) user="
                 << claims->user_id << " space=" << spaceId;
        conn->forceClose();
        return;
    }

    // 3. 建立到边侧 /ws 的客户端连接，原样转发 token + space_id。
    auto edge = drogon::WebSocketClient::newWebSocketClient(edgeWsUrl());
    auto ctx = std::make_shared<ProxyContext>();
    ctx->edge = edge;
    conn->setContext(ctx);

    // 边侧 → 小程序：把 device_update / cmd_response / ping 原样回推。
    edge->setMessageHandler(
        [conn](std::string&& msg, const drogon::WebSocketClientPtr&,
               const drogon::WebSocketMessageType& t) {
            if (conn->connected()) {
                conn->send(msg, t);
            }
        });
    // 边侧断开 → 关掉小程序侧连接。
    edge->setConnectionClosedHandler(
        [conn](const drogon::WebSocketClientPtr&) {
            if (conn->connected()) {
                conn->forceClose();
            }
        });

    auto up = drogon::HttpRequest::newHttpRequest();
    up->setPath("/ws");
    up->setParameter("token", token);
    up->setParameter("space_id", spaceId);
    edge->connectToServer(
        up, [conn](drogon::ReqResult result, const drogon::HttpResponsePtr&,
                   const drogon::WebSocketClientPtr&) {
            if (result != drogon::ReqResult::Ok && conn->connected()) {
                LOG_WARN << "ws proxy: edge /ws connect failed";
                conn->forceClose();
            }
        });
}

void WsProxyController::handleNewMessage(
    const drogon::WebSocketConnectionPtr& conn,
    std::string&& message,
    const drogon::WebSocketMessageType& type) {
    auto ctx = conn->getContext<ProxyContext>();
    if (!ctx || !ctx->edge) {
        return;
    }
    auto edgeConn = ctx->edge->getConnection();
    if (edgeConn && edgeConn->connected()) {
        edgeConn->send(message, type);
    }
}

void WsProxyController::handleConnectionClosed(
    const drogon::WebSocketConnectionPtr& conn) {
    auto ctx = conn->getContext<ProxyContext>();
    if (ctx && ctx->edge) {
        ctx->edge->stop();
    }
}
