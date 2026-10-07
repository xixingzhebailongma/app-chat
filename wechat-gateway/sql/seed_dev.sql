-- ============================================================================
-- 开发演示数据（仅开发环境手动执行：psql ... -f seed_dev.sql）。
-- 生产不执行——spaces/alerts 由 go-backend 或运营写入，user_roles 由
-- go-backend 经同步接口写入，user_spaces 随真实绑定产生。
-- 全部 ON CONFLICT DO NOTHING，可重复执行。
-- ============================================================================

INSERT INTO spaces (space_id, name, type) VALUES
    ('spc_a8acdd5c',  'A101 教室',      'standard'),
    ('spc_std_a102',  'A102 教室',      'standard'),
    ('spc_b7bee44d',  '多媒体报告厅',   'lecture'),
    ('spc_office_1',  '教师办公室',     'office'),
    ('spc_gym_1',     '体育馆',         'gym'),
    ('spc_lab_1',     '化学实验室',     'lab'),
    ('spc_lib_1',     '图书馆',         'library'),
    ('spc_canteen_1', '学生食堂',       'canteen')
ON CONFLICT (space_id) DO NOTHING;

-- 告警种子：device_id 为空串 = 纯传感器告警（无关联设备，前端隐藏「去控制设备」按钮）。
INSERT INTO alert_handles (alert_id, space_id, event_type, status, operator_id, remark, device_id, device_label) VALUES
    ('alt_1', 'spc_a8acdd5c', 'device_offline',    'unhandled', '',          '',      'dev_fuhe_a101',  'A101 综合屏'),
    ('alt_2', 'spc_b7bee44d', 'sensor_threshold',  'unhandled', '',          '',      '',               ''),
    ('alt_3', 'spc_a8acdd5c', 'face_login_failed', 'resolved',  'u_admin_1', '已处理', 'dev_face_a101',  'A101 人脸机'),
    ('alt_4', 'spc_office_1', 'device_offline',    'unhandled', '',          '',      'dev_enc_office', '教师办公室 编解码器'),
    ('alt_5', 'spc_gym_1',    'sensor_threshold',  'unhandled', '',          '',      '',               '')
ON CONFLICT (alert_id) DO NOTHING;

-- 教师-空间绑定（每位教师绑定一个空间，用于空间边界校验 + usersBySpace）。
INSERT INTO user_spaces (user_id, space_id) VALUES
    ('u_teacher_1', 'spc_a8acdd5c'),
    ('u_teacher_2', 'spc_b7bee44d')
ON CONFLICT (user_id, space_id) DO NOTHING;

-- 角色只读模型（演示数据；生产由 go-backend 同步写入）。
INSERT INTO user_roles (user_id, role) VALUES
    ('u_admin_1',   'admin'),
    ('u_admin_2',   'admin'),
    ('u_teacher_1', 'teacher'),
    ('u_teacher_2', 'teacher')
ON CONFLICT (user_id, role) DO NOTHING;

-- 小程序绑定（补姓名，供告警 operator_name / 运维日志 operator_name 解析）。
-- 生产由登录/bind 流程写入；此处仅为开发演示，让 alt_3 的 operator_id 能解析出姓名。
-- wechat_bindings 有两个唯一约束 (channel,openid)/(channel,user_id)，故用无目标的
-- ON CONFLICT DO NOTHING 同时兜住两类冲突。
INSERT INTO wechat_bindings (channel, openid, user_id, role, name) VALUES
    ('miniapp', 'openid_admin_seed',    'u_admin_1',   'admin',   '管理员'),
    ('miniapp', 'openid_teacher1_seed', 'u_teacher_1', 'teacher', '李老师'),
    ('miniapp', 'openid_teacher2_seed', 'u_teacher_2', 'teacher', '王老师')
ON CONFLICT DO NOTHING;

-- 进出记录样例（event_id 各不相同，否则 ON CONFLICT DO NOTHING 会静默吞掉）。
INSERT INTO access_records (event_id, space_id, user_id, name, auth_type, result, device_id, occurred_at) VALUES
    ('evt_seed_0001', 'spc_a8acdd5c', 'u_teacher_1', '李老师', 'face', 'login',  'dev_1a2b3c4d', now() - interval '10 minutes'),
    ('evt_seed_0002', 'spc_a8acdd5c', '',            '陌生人',  'face', 'denied', 'dev_1a2b3c4d', now() - interval '9 minutes'),
    ('evt_seed_0003', 'spc_b7bee44d', 'u_teacher_2', '王老师', 'face', 'login',  'dev_55aa66bb', now() - interval '8 minutes'),
    ('evt_seed_0004', 'spc_b7bee44d', 'u_teacher_2', '王老师', 'card', 'login',  'dev_55aa66bb', now() - interval '5 minutes')
ON CONFLICT (event_id) DO NOTHING;

-- 家长到校通知花名册（演示数据；生产由学校花名册经 /internal/student-parents/sync 回灌）。
INSERT INTO student_parents (student_no, student_name, parent_phone, relation, status) VALUES
    ('20260101', '张三', '13800000000', '父亲', 'active')
ON CONFLICT (student_no, parent_phone) DO NOTHING;
