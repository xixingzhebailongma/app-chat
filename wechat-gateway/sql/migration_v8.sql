-- 运维操作日志（设计文档 7.5 运维闭环 + 7.4② 场景控制）。
-- op_type 通用化：当前仅 scene_execute 使用，后续管理员远程控制、
-- 告警处置等运维操作复用同一张表。
CREATE TABLE IF NOT EXISTS operation_logs (
    id BIGSERIAL PRIMARY KEY,
    op_type       VARCHAR(32) NOT NULL,          -- 'scene_execute'
    user_id       VARCHAR(64) NOT NULL,
    space_id      VARCHAR(64) NOT NULL,
    scene_id      VARCHAR(32) NOT NULL,
    success_count INT NOT NULL DEFAULT 0,
    failed_count  INT NOT NULL DEFAULT 0,
    detail        JSONB,
    created_at    TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_operation_logs_space_time
    ON operation_logs(space_id, created_at DESC);
