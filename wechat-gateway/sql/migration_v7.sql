-- migration_v7：告警历史查询筛选/分页的组合索引（设计文档 7.5④ 按时间/教室/类型筛选）。
-- 对应 PgAlertRepository::query/count 的 WHERE space_id/event_type/status/created_at 条件。
CREATE INDEX IF NOT EXISTS idx_alert_handles_filter
    ON alert_handles (space_id, event_type, status, created_at DESC);
