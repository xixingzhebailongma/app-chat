#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// ai_conversations 表的一行（见 sql/migration_v18.sql）。会话按 user_id 隔离。
struct AiConversation {
    std::string conversation_id;
    std::string user_id;
    std::string title;
    std::string created_at;
    std::string updated_at;
};

// ai_messages 表的一行。只落最终 user/assistant 消息，不落工具中间消息。
struct AiMessage {
    std::string role;      // user / assistant
    std::string content;
    std::string created_at;
};

class AiConversationRepository {
public:
    virtual ~AiConversationRepository() = default;

    virtual std::optional<AiConversation> findConversation(
        const std::string& conversation_id) const = 0;

    // 仅返回有消息的会话（空会话不展示），按 updated_at 降序。
    virtual std::vector<AiConversation> listConversations(
        const std::string& user_id) const = 0;

    // 按时间正序返回消息；limit <= 0 返回全部，limit > 0 返回最近 limit 条。
    virtual std::vector<AiMessage> listMessages(const std::string& conversation_id,
                                                int limit) const = 0;

    // 原子保存一轮：ensureConversation 时先确保会话行存在（ON CONFLICT 跳过），
    // 再插入 messages 并刷新 updated_at。全部在同一事务内。
    virtual void saveTurn(const AiConversation& conv, bool ensureConversation,
                          const std::vector<AiMessage>& messages) = 0;

    // 删除会话（ai_messages 经外键 ON DELETE CASCADE 一并删除）。
    virtual void deleteConversation(const std::string& conversation_id) = 0;
};

// 内存 mock。在开发装配 / 测试中使用。
class InMemoryAiConversationRepository : public AiConversationRepository {
public:
    std::optional<AiConversation> findConversation(
        const std::string& conversation_id) const override {
        const auto it = convs_.find(conversation_id);
        if (it == convs_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    std::vector<AiConversation> listConversations(
        const std::string& user_id) const override {
        // 仅返回有消息的会话（空会话不展示）。内存 mock 不保证顺序。
        std::vector<AiConversation> out;
        for (const auto& [_, conv] : convs_) {
            if (conv.user_id != user_id) {
                continue;
            }
            if (msgs_.count(conv.conversation_id) == 0) {
                continue;
            }
            out.push_back(conv);
        }
        return out;
    }

    std::vector<AiMessage> listMessages(const std::string& conversation_id,
                                        int limit) const override {
        const auto it = msgs_.find(conversation_id);
        if (it == msgs_.end()) {
            return {};
        }
        const auto& all = it->second;
        size_t start = 0;
        if (limit > 0 && all.size() > static_cast<size_t>(limit)) {
            start = all.size() - static_cast<size_t>(limit);
        }
        return std::vector<AiMessage>(all.begin() + start, all.end());
    }

    void saveTurn(const AiConversation& conv, bool ensureConversation,
                  const std::vector<AiMessage>& messages) override {
        const bool exists = convs_.count(conv.conversation_id) != 0;
        if (ensureConversation && !exists) {
            AiConversation c = conv;
            c.created_at = "now";
            c.updated_at = "now";
            convs_[conv.conversation_id] = c;
        }
        auto& stored = msgs_[conv.conversation_id];
        for (const auto& m : messages) {
            stored.push_back(m);
        }
        if (exists || ensureConversation) {
            convs_[conv.conversation_id].updated_at = "now";
        }
    }

    void deleteConversation(const std::string& conversation_id) override {
        convs_.erase(conversation_id);
        msgs_.erase(conversation_id);
    }

private:
    std::unordered_map<std::string, AiConversation> convs_;
    std::unordered_map<std::string, std::vector<AiMessage>> msgs_;
};
