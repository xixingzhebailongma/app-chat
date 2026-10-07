-- ============================================================================
-- 十三、告警补设备字段（7.5② 告警详情「远程控制设备」定位）。
-- 生产侧 upsert 原本丢弃 device 信息，现补存；device_type 不存，前端按
-- device_id 在设备列表匹配。空字符串 = 无关联设备（如纯传感器告警）。
-- 在 migration_v1..v12 之上执行。
-- ============================================================================
ALTER TABLE alert_handles
    ADD COLUMN IF NOT EXISTS device_id VARCHAR(64) NOT NULL DEFAULT '';
ALTER TABLE alert_handles
    ADD COLUMN IF NOT EXISTS device_label VARCHAR(128) NOT NULL DEFAULT '';
