-- ============================================================================
-- 十一、通知方向解析表：user_id + channel -> 渠道外部地址（只服务教师/管理员，
-- 家长到校走 parent_student_bindings，不并入此表）。
-- 在 migration_v1..v10 之上执行。
-- ============================================================================
CREATE TABLE IF NOT EXISTS user_notify_bindings (
    id             BIGSERIAL   PRIMARY KEY,
    user_id        VARCHAR(64) NOT NULL,   -- 与 wechat_bindings/user_spaces/subscriptions 一致
    channel        VARCHAR(32) NOT NULL,   -- wechat_miniapp / sms / dingtalk / wecom
    external_id    VARCHAR(128) NOT NULL,  -- openid / phone / dingtalk userid / wecom userid
    external_extra JSONB       NOT NULL DEFAULT '{}'::jsonb,
    verified_at    TIMESTAMPTZ,
    created_at     TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at     TIMESTAMPTZ NOT NULL DEFAULT now(),
    CONSTRAINT uq_notify_binding_channel_user UNIQUE (channel, user_id)
);

CREATE INDEX IF NOT EXISTS idx_notify_binding_user
    ON user_notify_bindings (user_id);

-- 身份类渠道（miniapp/dingtalk/wecom）一人一号，external_id 唯一；sms 手机号
-- 允许多人共用（值班手机），故不做表级 UNIQUE(channel, external_id)，改用「排除
-- sms」的部分唯一索引。
CREATE UNIQUE INDEX IF NOT EXISTS uq_notify_binding_identity_ext
    ON user_notify_bindings (channel, external_id)
    WHERE channel <> 'sms';

-- 回填：登录方向 wechat_bindings.channel='miniapp' 映射到通知方向 'wechat_miniapp'
INSERT INTO user_notify_bindings (user_id, channel, external_id, verified_at)
SELECT user_id, 'wechat_miniapp', openid, now()
FROM wechat_bindings WHERE channel = 'miniapp'
ON CONFLICT (channel, user_id) DO NOTHING;
