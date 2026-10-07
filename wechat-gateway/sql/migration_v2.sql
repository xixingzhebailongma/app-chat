-- ============================================================================
-- 十三、降级策略 — pending_events 重试队列 + notify_dispatch 幂等账本
-- (在 migration_v1.sql 之上增量执行)
-- ============================================================================

-- 1. pending_events 补列：状态机 + 领取信息（多副本 worker 安全）
--    status: pending -> processing -> (deleted | pending | dead_letter)
--    claimed_at/claimed_by 用于崩溃后回收超时的 processing 行。
ALTER TABLE pending_events
    ADD COLUMN IF NOT EXISTS status       VARCHAR(16) NOT NULL DEFAULT 'pending'
        CHECK (status IN ('pending', 'processing', 'dead_letter')),
    ADD COLUMN IF NOT EXISTS claimed_at   TIMESTAMPTZ,
    ADD COLUMN IF NOT EXISTS claimed_by   VARCHAR(128),
    ADD COLUMN IF NOT EXISTS last_error   TEXT;

-- 领取索引：worker 只扫 pending 且到期的行（FOR UPDATE SKIP LOCKED 在此上执行）
DROP INDEX IF EXISTS idx_pe_retry;
CREATE INDEX idx_pe_retry
    ON pending_events (next_retry_at)
    WHERE status = 'pending';

-- 2. notify_dispatch 幂等账本：一行一条「用户 × 渠道」的投递记录。
--    唯一约束 (event_id, target_user, channel) 保证同一事件对同一用户经同一
--    渠道只投递一次（重试/并发 worker 都靠 INSERT ... ON CONFLICT DO NOTHING）。
CREATE TABLE IF NOT EXISTS notify_dispatch (
    id          BIGSERIAL   PRIMARY KEY,
    event_id    VARCHAR(64) NOT NULL,
    target_user VARCHAR(64) NOT NULL,
    channel     VARCHAR(32) NOT NULL,
    notify_id   VARCHAR(64) NOT NULL,
    created_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
    CONSTRAINT uq_dispatch_event_user_channel
        UNIQUE (event_id, target_user, channel)
);

CREATE INDEX IF NOT EXISTS idx_nd_event
    ON notify_dispatch (event_id);
