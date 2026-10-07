-- 到校通知日志（7.8）。家长到校通知专用日志，独立于小程序告警的
-- notify_logs / notify_attempts（那些服务告警推送，不服务 OA 到校）。
--
-- 唯一键 (event_id, parent_openid) 现阶段只保护「同一事件至多一条事件级日志」
-- （parent_openid 用占位 '__event_level__'）；逐家长幂等是预留，等真实发送
-- 接入、逐家长写日志后再启用。
CREATE TABLE IF NOT EXISTS oa_notify_logs (
    id            BIGSERIAL    PRIMARY KEY,
    event_id      VARCHAR(64)  NOT NULL,          -- == 响应 notify_id（与日志同值）
    parent_openid VARCHAR(128) NOT NULL,          -- 事件级日志用占位 '__event_level__'
    student_no    VARCHAR(64)  NOT NULL,
    space_id      VARCHAR(64),                    -- 请求暂缺 space_id，先可空（业务去重待补）
    event_type    VARCHAR(16)  NOT NULL DEFAULT 'arrival',
    template_id   VARCHAR(64),
    status        VARCHAR(24)  NOT NULL
        CHECK (status IN ('pending', 'success', 'failed', 'skipped',
                          'no_parent_binding', 'duplicate')),
    reason        TEXT,                           -- channel_disabled / mock / no_parent_binding / ''
    request_body  JSONB,
    response_body JSONB,
    retry_count   INT          NOT NULL DEFAULT 0,
    sent_at       TIMESTAMPTZ,
    created_at    TIMESTAMPTZ  NOT NULL DEFAULT now(),
    CONSTRAINT uq_oa_notify UNIQUE (event_id, parent_openid)
);

CREATE INDEX IF NOT EXISTS idx_oa_notify_event
    ON oa_notify_logs (event_id);

CREATE INDEX IF NOT EXISTS idx_oa_notify_stu_sp
    ON oa_notify_logs (student_no, space_id, created_at DESC);
