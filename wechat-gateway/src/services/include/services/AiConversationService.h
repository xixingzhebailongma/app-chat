#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "db/AiConversationRepository.h"

// AI 会话记忆服务：包装 AiConversationRepository，提供按用户隔离的会话
// 读取/删除与整轮原子保存，并负责 conversation_id 生成。归属校验（非本人
// 视为不存在）在此层完成。
class AiConversationService {
public:
    explicit AiConversationService(
        std::shared_ptr<AiConversationRepository> repo);

    // 按 id 查会话并校验归属；不存在或非本人返回 nullopt。
    std::optional<AiConversation> get(const std::string& userId,
                                      const std::string& conversationId) const;

    // 本人会话列表（仅含非空会话，按最近更新降序）。
    std::vector<AiConversation> list(const std::string& userId) const;

    // 纯读消息（调用方需先经 get() 校验归属）。
    std::vector<AiMessage> messages(const std::string& conversationId,
                                    int limit) const;

    // 整轮保存（user 消息 + assistant 回复，原子）。ensureConversation=true
    // 时先确保会话行存在（新建会话）。
    void saveTurn(const AiConversation& conv, bool ensureConversation,
                  const std::vector<AiMessage>& messages);

    // 删除本人会话；不存在或非本人返回 false。
    bool remove(const std::string& userId, const std::string& conversationId);

    static std::string newConversationId();

private:
    std::shared_ptr<AiConversationRepository> repo_;
};
