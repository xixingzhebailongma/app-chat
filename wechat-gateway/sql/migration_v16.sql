-- ============================================================================
-- 十六、管理员增删改教室（网关为空间权威来源）：
--   spaces 增加 enabled（软删除）+ source（edge/admin），用于区分来源与同步策略。
-- 在 migration_v1..v15 之上执行。
-- ============================================================================
ALTER TABLE spaces
    ADD COLUMN IF NOT EXISTS enabled BOOLEAN NOT NULL DEFAULT TRUE;
ALTER TABLE spaces
    ADD COLUMN IF NOT EXISTS source VARCHAR(16) NOT NULL DEFAULT 'edge';
