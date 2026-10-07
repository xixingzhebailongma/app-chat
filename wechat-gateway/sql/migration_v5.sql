-- ============================================================================
-- 十六、订阅授权：用户级布尔 → 模板级额度（重建式迁移）
-- 在 migration_v1..v4 之上执行。
-- ============================================================================
--
-- 影响：旧 subscriptions(user_id PK, subscribed BOOLEAN) 只有「全局开/关」，
-- 不含任何 per-template 授权信息。新模型按 (user_id, template_id) 记录额度：
--   quota = -1  长期订阅（不限次）
--   quota =  0  无授权（无行等价）
--   quota >  0  一次性订阅剩余次数
-- 迁移后所有用户需在小程序内重新授权一次（opt-out → opt-in 的必然结果）。
--
-- 本迁移为 DROP + CREATE：仅限开发/预生产。若未来生产已有存量订阅数据，
-- 请改用「CREATE new + RENAME 保留旧表」的重建式迁移。subscriptions 无外键依赖。
DROP TABLE IF EXISTS subscriptions;
CREATE TABLE subscriptions (
    user_id     VARCHAR(64)  NOT NULL,
    template_id VARCHAR(128) NOT NULL,
    quota       INT          NOT NULL DEFAULT 0,
    updated_at  TIMESTAMPTZ  NOT NULL DEFAULT now(),
    PRIMARY KEY (user_id, template_id)
);
