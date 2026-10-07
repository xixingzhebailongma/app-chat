-- ============================================================================
-- 十四、角色只读模型 + 日志补列
-- (在 migration_v1.sql、migration_v2.sql 之上增量执行，幂等)
-- ============================================================================

-- 1. user_roles：角色→用户的只读模型/缓存。
--    权威源在 go-backend；网关只读（usersByRole 查询），由 go-backend 在
--    登录/角色变更时通过 POST /internal/user-roles/sync 全量替换写入。
CREATE TABLE IF NOT EXISTS user_roles (
    user_id    VARCHAR(64) NOT NULL,
    role       VARCHAR(32) NOT NULL,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (user_id, role)
);

CREATE INDEX IF NOT EXISTS idx_user_roles_role
    ON user_roles (role);

-- 2. 按用户路由（设计文档 13.3）需要把每条渠道尝试的 target_user 也落库。
--    ChannelAttempt.target_user 为空表示事件级分发。
ALTER TABLE notify_attempts
    ADD COLUMN IF NOT EXISTS target_user VARCHAR(64);

-- 3. 补全 notify_logs 的事件属性（NotifySendRequest 7.6 段），
--    让通知日志与请求完整对应（可空，历史行不受影响）。
ALTER TABLE notify_logs
    ADD COLUMN IF NOT EXISTS transition TEXT,
    ADD COLUMN IF NOT EXISTS duration_sec INT,
    ADD COLUMN IF NOT EXISTS consecutive INT,
    ADD COLUMN IF NOT EXISTS offline_minutes INT;
