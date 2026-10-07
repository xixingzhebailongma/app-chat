-- ============================================================================
-- 十七、进出记录：access_records 表（在 migration_v1..v5 之上执行）。
-- ============================================================================
-- 数据由边侧经 POST /internal/access-records 摄取（event_id 幂等），
-- 由网关 GET /api/miniapp/access-records 按空间/日期/分页查询。
--
-- 脱敏在响应层：表存 user_id 供将来按人统计，但 API 不返回；不存人脸图/feature。
CREATE TABLE IF NOT EXISTS access_records (
    id          BIGSERIAL    PRIMARY KEY,
    event_id    VARCHAR(64)  NOT NULL UNIQUE,          -- 边侧事件唯一 ID，防重推
    space_id    VARCHAR(64)  NOT NULL,
    user_id     VARCHAR(64)  NOT NULL DEFAULT '',      -- 存储但不返回
    name        VARCHAR(128) NOT NULL DEFAULT '',
    auth_type   VARCHAR(16)  NOT NULL DEFAULT 'face',  -- face / card
    result      VARCHAR(16)  NOT NULL DEFAULT 'login', -- login / denied
    device_id   VARCHAR(64)  NOT NULL DEFAULT '',
    occurred_at TIMESTAMPTZ  NOT NULL,
    created_at  TIMESTAMPTZ  NOT NULL DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_access_records_space_time
    ON access_records (space_id, occurred_at DESC);
