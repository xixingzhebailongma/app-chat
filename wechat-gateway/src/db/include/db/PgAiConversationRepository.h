#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "db/AiConversationRepository.h"

class PgPool;

// 基于 PostgreSQL 的 AI 会话仓库（sql/migration_v18.sql）。
// 仅在定义 HAS_LIBPQ 时编译（见 CMakeLists.txt）。
class PgAiConversationRepository : public AiConversationRepository {
public:
    explicit PgAiConversationRepository(std::shared_ptr<PgPool> pool);

    std::optional<AiConversation> findConversation(
        const std::string& conversation_id) const override;
    std::vector<AiConversation> listConversations(
        const std::string& user_id) const override;
    std::vector<AiMessage> listMessages(const std::string& conversation_id,
                                        int limit) const override;
    void saveTurn(const AiConversation& conv, bool ensureConversation,
                  const std::vector<AiMessage>& messages) override;
    void deleteConversation(const std::string& conversation_id) override;

private:
    std::shared_ptr<PgPool> pool_;
};
