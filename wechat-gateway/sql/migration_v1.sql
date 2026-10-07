-- wechat_bindings — maps a WeChat miniapp openid to a go-backend user.
-- Written by the gateway at bind time (design doc 六). role and name are
-- persisted alongside user_id so a later login can issue a JWT locally without
-- calling go-backend (which would break the "本地验证，不回调 go-backend" rule).

CREATE TABLE IF NOT EXISTS wechat_bindings (
    id         BIGSERIAL    PRIMARY KEY,
    channel    VARCHAR(32)  NOT NULL DEFAULT 'miniapp',
    openid     VARCHAR(128) NOT NULL,
    user_id    VARCHAR(64)  NOT NULL,
    role       VARCHAR(32)  NOT NULL DEFAULT 'teacher',
    name       VARCHAR(128) NOT NULL DEFAULT '',
    created_at TIMESTAMPTZ  NOT NULL DEFAULT now(),
    updated_at TIMESTAMPTZ  NOT NULL DEFAULT now(),

    CONSTRAINT uq_wechat_bindings_channel_openid UNIQUE (channel, openid),
    CONSTRAINT uq_wechat_bindings_channel_userid UNIQUE (channel, user_id)
);

CREATE INDEX IF NOT EXISTS idx_wechat_bindings_userid
    ON wechat_bindings (user_id);


-- ============================================================================
-- 以下为追加的表（public schema，无前缀）。
-- 约定：user_id / operator_id 等对外引用统一 VARCHAR(64)，存 go-backend 的
-- user_id 十进制字符串；不建跨 schema 外键。
-- ============================================================================

-- wechat_bindings 补充 unionid（跨渠道同一用户去重用）
ALTER TABLE wechat_bindings
    ADD COLUMN IF NOT EXISTS unionid VARCHAR(64);

-- 教师-教室绑定
CREATE TABLE IF NOT EXISTS user_spaces (
    user_id    VARCHAR(64) NOT NULL,
    space_id   VARCHAR(64) NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (user_id, space_id)
);

CREATE INDEX IF NOT EXISTS idx_user_spaces_space
    ON user_spaces (space_id);

-- 家长-学生绑定（支持一个学生绑定多个家长）
CREATE TABLE IF NOT EXISTS parent_student_bindings (
    id               BIGSERIAL   PRIMARY KEY,
    student_no       VARCHAR(64) NOT NULL,
    parent_openid_oa VARCHAR(64) NOT NULL,
    phone            VARCHAR(20) NOT NULL,
    verified_at      TIMESTAMPTZ NOT NULL DEFAULT now(),
    CONSTRAINT uq_parent_student UNIQUE (student_no, parent_openid_oa)
);

CREATE INDEX IF NOT EXISTS idx_psb_student
    ON parent_student_bindings (student_no);

-- 通知日志：一次通知一行，只记最终状态；每个渠道的尝试结果见 notify_attempts
CREATE TABLE IF NOT EXISTS notify_logs (
    notify_id      VARCHAR(64)  PRIMARY KEY,
    event_id       VARCHAR(64),
    event_type     VARCHAR(64)  NOT NULL,
    space_id       VARCHAR(64),
    device_id      VARCHAR(64),
    device_label   VARCHAR(128),
    severity       VARCHAR(16)  NOT NULL DEFAULT 'info',
    content        TEXT,
    target_roles   JSONB,
    space_teachers BOOLEAN      NOT NULL DEFAULT false,
    final_status   VARCHAR(16)  NOT NULL
        CHECK (final_status IN ('success', 'partial', 'failed', 'skipped')),
    created_at     TIMESTAMPTZ  NOT NULL DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_nl_device_time
    ON notify_logs (device_id, event_type, created_at DESC);

CREATE INDEX IF NOT EXISTS idx_nl_event
    ON notify_logs (event_id);

-- 通知渠道尝试：一行一渠道，保留尝试顺序与每渠道结果（fallback 标记也在此）
CREATE TABLE IF NOT EXISTS notify_attempts (
    id          BIGSERIAL   PRIMARY KEY,
    notify_id   VARCHAR(64) NOT NULL REFERENCES notify_logs (notify_id) ON DELETE CASCADE,
    channel     VARCHAR(32) NOT NULL,
    seq         SMALLINT    NOT NULL DEFAULT 1,
    status      VARCHAR(16) NOT NULL CHECK (status IN ('success', 'failed')),
    error_msg   TEXT,
    is_fallback BOOLEAN     NOT NULL DEFAULT false,
    created_at  TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_na_notify
    ON notify_attempts (notify_id, seq);

-- 告警处理状态
CREATE TABLE IF NOT EXISTS alert_handles (
    alert_id    VARCHAR(64) PRIMARY KEY,
    space_id    VARCHAR(64),
    event_type  VARCHAR(64),
    status      VARCHAR(16) NOT NULL DEFAULT 'unhandled'
        CHECK (status IN ('unhandled', 'handling', 'resolved')),
    operator_id VARCHAR(64),
    remark      TEXT,
    created_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at  TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_ah_unhandled
    ON alert_handles (status) WHERE status = 'unhandled';

-- 待处理队列（降级）
CREATE TABLE IF NOT EXISTS pending_events (
    id            BIGSERIAL   PRIMARY KEY,
    payload       JSONB       NOT NULL,
    retry_count   INT         NOT NULL DEFAULT 0,
    next_retry_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    created_at    TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_pe_retry
    ON pending_events (next_retry_at) WHERE retry_count < 5;

-- 空间（教室/区域）基础表：GET /api/miniapp/spaces 的数据源
CREATE TABLE IF NOT EXISTS spaces (
    space_id   VARCHAR(64)  PRIMARY KEY,
    name       VARCHAR(128) NOT NULL DEFAULT '',
    type       VARCHAR(32)  NOT NULL DEFAULT 'standard',
    created_at TIMESTAMPTZ  NOT NULL DEFAULT now()
);

-- 小程序订阅授权：记录用户是否授权订阅消息推送
CREATE TABLE IF NOT EXISTS subscriptions (
    user_id    VARCHAR(64) PRIMARY KEY,
    subscribed BOOLEAN     NOT NULL DEFAULT true,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
