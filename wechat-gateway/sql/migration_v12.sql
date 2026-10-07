-- ============================================================================
-- 十二、家长到校通知（7.8）：学生花名册（学校权威数据）。
-- 一行一家长，(student_no, parent_phone) 唯一；status 随学校侧变更同步。
-- 绑定校验（OaBindService::confirm）以本表为权威来源；全量同步走
-- POST /internal/student-parents/sync。在 migration_v1..v11 之上执行。
-- ============================================================================
CREATE TABLE IF NOT EXISTS student_parents (
    id           BIGSERIAL   PRIMARY KEY,
    student_no   VARCHAR(64) NOT NULL,
    student_name VARCHAR(128) NOT NULL DEFAULT '',
    parent_phone VARCHAR(20) NOT NULL,
    relation     VARCHAR(32) NOT NULL DEFAULT '其他',  -- 父亲/母亲/其他
    status       VARCHAR(16) NOT NULL DEFAULT 'active'
        CHECK (status IN ('active', 'inactive')),
    created_at   TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at   TIMESTAMPTZ NOT NULL DEFAULT now(),
    CONSTRAINT uq_student_parent UNIQUE (student_no, parent_phone)
);

CREATE INDEX IF NOT EXISTS idx_student_parent_student
    ON student_parents (student_no, status);
