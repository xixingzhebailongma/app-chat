-- ============================================================================
-- 十八、AI 会话记忆（第一阶段：会话历史持久化，不做摘要/RAG 长期记忆）：
--   ai_conversations（会话目录，按用户隔离）+ ai_messages（会话内 user/assistant
--   消息，BIGSERIAL 排序）。只落最终 user/assistant 消息，不落工具调用中间消息；
--   「最近 20 条上下文」由网关读取时截断，不在落库时裁剪。
-- 在 migration_v1..v17 之上执行。
-- ============================================================================
CREATE TABLE IF NOT EXISTS ai_conversations (
    conversation_id VARCHAR(64)  PRIMARY KEY,
    user_id         VARCHAR(64)  NOT NULL,
    title           VARCHAR(128) NOT NULL DEFAULT '',
    created_at      TIMESTAMPTZ  NOT NULL DEFAULT now(),
    updated_at      TIMESTAMPTZ  NOT NULL DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_ai_conversations_user
    ON ai_conversations (user_id, updated_at DESC);

CREATE TABLE IF NOT EXISTS ai_messages (
    id              BIGSERIAL    PRIMARY KEY,
    conversation_id VARCHAR(64)  NOT NULL
        REFERENCES ai_conversations (conversation_id) ON DELETE CASCADE,
    role            VARCHAR(16)  NOT NULL
        CHECK (role IN ('user', 'assistant')),   -- 硬约束：不落工具中间消息
    content         TEXT         NOT NULL,
    created_at      TIMESTAMPTZ  NOT NULL DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_ai_messages_conv
    ON ai_messages (conversation_id, id);
