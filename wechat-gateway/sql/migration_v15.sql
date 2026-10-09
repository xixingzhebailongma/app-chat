-- ============================================================================
-- 十五、空间类型管理（管理员自定义空间类型，前端动态拉取）：
--   空间类型标签 / 顺序 / 启停不再硬编码在前端，落到 space_types 表；
--   code 为唯一业务键（spaces.type 引用），种子对齐前端 format.js 的
--   SPACE_TYPES 与现有 spaces.type 的 7 种类型。
-- 在 migration_v1..v14 之上执行。
-- ============================================================================
CREATE TABLE IF NOT EXISTS space_types (
    code        VARCHAR(64)  PRIMARY KEY,
    name        VARCHAR(128) NOT NULL DEFAULT '',
    sort_order  INTEGER      NOT NULL DEFAULT 0,
    enabled     BOOLEAN      NOT NULL DEFAULT TRUE,
    icon        VARCHAR(64)  NOT NULL DEFAULT '',
    created_at  TIMESTAMPTZ  NOT NULL DEFAULT now(),
    updated_at  TIMESTAMPTZ  NOT NULL DEFAULT now()
);

INSERT INTO space_types (code, name, sort_order) VALUES
    ('standard', '标准教室', 0),
    ('lecture', '多媒体报告厅', 1),
    ('office', '办公室', 2),
    ('gym', '体育馆', 3),
    ('lab', '实验室', 4),
    ('library', '图书馆', 5),
    ('canteen', '食堂', 6)
ON CONFLICT (code) DO NOTHING;
