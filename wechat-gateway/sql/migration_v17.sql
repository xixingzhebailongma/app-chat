-- ============================================================================
-- 十七、教师自定义场景（按教室共享、指定设备+状态）：
--   scenes 表（场景目录）+ spaces 增加 active_scene_id（当前激活场景）/
--   scenes_seeded（默认场景是否已种入）。
-- 在 migration_v1..v16 之上执行。
-- ============================================================================
CREATE TABLE IF NOT EXISTS scenes (
    scene_id      VARCHAR(64)  PRIMARY KEY,
    space_id      VARCHAR(64)  NOT NULL,
    name          VARCHAR(128) NOT NULL DEFAULT '',
    kind          VARCHAR(16)  NOT NULL DEFAULT 'custom',  -- custom / all_on / all_off
    device_states TEXT         NOT NULL DEFAULT '[]',      -- custom 用：JSON [{device_id,device_type,command}]
    created_at    TIMESTAMPTZ  NOT NULL DEFAULT now(),
    updated_at    TIMESTAMPTZ  NOT NULL DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_scenes_space ON scenes (space_id);

ALTER TABLE spaces
    ADD COLUMN IF NOT EXISTS active_scene_id VARCHAR(64) NOT NULL DEFAULT '';
ALTER TABLE spaces
    ADD COLUMN IF NOT EXISTS scenes_seeded BOOLEAN NOT NULL DEFAULT FALSE;
