-- ============================================================================
-- 十四、OA 到校通知运维收尾（最小可交付）：
--   ① 家长绑定可达性状态：取关/拒收后跳过推送，重绑重置为 active
--   ② oa_notify_logs 增加 errcode 结构化落库（不再折叠进 reason）
-- 在 migration_v1..v13 之上执行。
-- ============================================================================
ALTER TABLE parent_student_bindings
    ADD COLUMN IF NOT EXISTS reach_status VARCHAR(16) NOT NULL DEFAULT 'active'
        CHECK (reach_status IN ('active', 'unsubscribed', 'refused'));
ALTER TABLE parent_student_bindings
    ADD COLUMN IF NOT EXISTS last_unreachable_at TIMESTAMPTZ;

ALTER TABLE oa_notify_logs
    ADD COLUMN IF NOT EXISTS errcode INT NOT NULL DEFAULT 0;
