#include "services/AiConversationService.h"

#include <utility>

#include "utils/HttpResponseUtil.h"

AiConversationService::AiConversationService(
    std::shared_ptr<AiConversationRepository> repo)
    : repo_(std::move(repo)) {}

std::optional<AiConversation> AiConversationService::get(
    const std::string& userId, const std::string& conversationId) const {
    const auto conv = repo_->findConversation(conversationId);
    if (!conv || conv->user_id != userId) {
        return std::nullopt;
    }
    return conv;
}

std::vector<AiConversation> AiConversationService::list(
    const std::string& userId) const {
    return repo_->listConversations(userId);
}

std::vector<AiMessage> AiConversationService::messages(
    const std::string& conversationId, int limit) const {
    return repo_->listMessages(conversationId, limit);
}

void AiConversationService::saveTurn(const AiConversation& conv,
                                     bool ensureConversation,
                                     const std::vector<AiMessage>& messages) {
    repo_->saveTurn(conv, ensureConversation, messages);
}

bool AiConversationService::remove(const std::string& userId,
                                   const std::string& conversationId) {
    const auto conv = repo_->findConversation(conversationId);
    if (!conv || conv->user_id != userId) {
        return false;
    }
    repo_->deleteConversation(conversationId);
    return true;
}

std::string AiConversationService::newConversationId() {
    return http_util::newId("conv");
}
