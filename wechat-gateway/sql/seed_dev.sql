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
-- 覆盖全部 4 类事件 + 三种状态（unhandled/handling/resolved），分布在多个教室。
INSERT INTO alert_handles (alert_id, space_id, event_type, status, operator_id, remark, device_id, device_label) VALUES
    ('alt_1',  'spc_a8acdd5c',  'device_offline',     'unhandled', '',          '',       'dev_fuhe_a101',   'A101 综合屏'),
    ('alt_2',  'spc_b7bee44d',  'sensor_threshold',   'unhandled', '',          '',       '',                ''),
    ('alt_3',  'spc_a8acdd5c',  'face_login_failed',  'resolved',  'u_admin_1', '已处理', 'dev_face_a101',   'A101 人脸机'),
    ('alt_4',  'spc_office_1',  'device_offline',     'unhandled', '',          '',       'dev_enc_office',  '教师办公室 编解码器'),
    ('alt_5',  'spc_gym_1',     'sensor_threshold',   'unhandled', '',          '',       '',                ''),
    ('alt_6',  'spc_std_a102',  'device_offline',     'handling',  'u_admin_1', '正在排查', 'dev_cc33dd44',   'A102 电子班牌'),
    ('alt_7',  'spc_canteen_1', 'peripheral_offline', 'unhandled', '',          '',       'dev_ctn_sens',    '学生食堂 温湿度传感器'),
    ('alt_8',  'spc_lab_1',     'sensor_threshold',   'resolved',  'u_admin_2', '已复位', '',                ''),
    ('alt_9',  'spc_lib_1',     'face_login_failed',  'unhandled', '',          '',       'dev_face_lib',    '图书馆 人脸机'),
    ('alt_10', 'spc_a8acdd5c',  'sensor_threshold',   'unhandled', '',          '',       '',                ''),
    ('alt_11', 'spc_gym_1',     'device_offline',     'resolved',  'u_admin_1', '已修复', 'dev_gym_screen',  '体育馆 大屏'),
    ('alt_12', 'spc_std_a102',  'face_login_failed',  'unhandled', '',          '',       'dev_face_a102',   'A102 人脸机')
ON CONFLICT (alert_id) DO NOTHING;

-- 教师-空间绑定：每位教师管理多个教室（李老师 3 间、王老师 3 间）。
-- 用于空间边界校验 + usersBySpace；教师端「我的教室」据此展示。
INSERT INTO user_spaces (user_id, space_id) VALUES
    ('u_teacher_1', 'spc_a8acdd5c'),   -- 李老师：A101 教室
    ('u_teacher_1', 'spc_std_a102'),   -- 李老师：A102 教室
    ('u_teacher_1', 'spc_lab_1'),      -- 李老师：化学实验室
    ('u_teacher_2', 'spc_b7bee44d'),   -- 王老师：多媒体报告厅
    ('u_teacher_2', 'spc_gym_1'),      -- 王老师：体育馆
    ('u_teacher_2', 'spc_lib_1')       -- 王老师：图书馆
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
    ('evt_seed_0001', 'spc_a8acdd5c',  'u_teacher_1', '李老师', 'face', 'login',  'dev_1a2b3c4d', now() - interval '10 minutes'),
    ('evt_seed_0002', 'spc_a8acdd5c',  '',            '陌生人',  'face', 'denied', 'dev_1a2b3c4d', now() - interval '9 minutes'),
    ('evt_seed_0003', 'spc_b7bee44d',  'u_teacher_2', '王老师', 'face', 'login',  'dev_55aa66bb', now() - interval '8 minutes'),
    ('evt_seed_0004', 'spc_b7bee44d',  'u_teacher_2', '王老师', 'card', 'login',  'dev_55aa66bb', now() - interval '5 minutes'),
    ('evt_seed_0005', 'spc_a8acdd5c',  'u_teacher_1', '李老师', 'card', 'login',  'dev_d0or_a101', now() - interval '4 minutes'),
    ('evt_seed_0006', 'spc_std_a102',  'u_teacher_1', '李老师', 'face', 'login',  'dev_d0or_a102', now() - interval '30 minutes'),
    ('evt_seed_0007', 'spc_gym_1',     'u_teacher_2', '王老师', 'face', 'login',  'dev_gym_door',  now() - interval '1 hour'),
    ('evt_seed_0008', 'spc_lib_1',     '',            '访客',    'face', 'denied', 'dev_lib_door',  now() - interval '2 hours'),
    ('evt_seed_0009', 'spc_lab_1',     'u_teacher_1', '李老师', 'card', 'login',  'dev_lab_door',  now() - interval '3 hours'),
    ('evt_seed_0010', 'spc_canteen_1', '',            '学生',    'face', 'login',  'dev_ctn_door',  now() - interval '4 hours'),
    ('evt_seed_0011', 'spc_office_1',  'u_admin_1',   '管理员',  'card', 'login',  'dev_off_door',  now() - interval '5 hours'),
    ('evt_seed_0012', 'spc_a8acdd5c',  '',            '未知',    'face', 'denied', 'dev_1a2b3c4d', now() - interval '6 hours')
ON CONFLICT (event_id) DO NOTHING;

-- 家长到校通知花名册（演示数据；生产由学校花名册经 /internal/student-parents/sync 回灌）。
INSERT INTO student_parents (student_no, student_name, parent_phone, relation, status) VALUES
    ('20260101', '张三', '13800000000', '父亲', 'active'),
    ('20260102', '李四', '13800000001', '母亲', 'active'),
    ('20260103', '王五', '13800000002', '父亲', 'active'),
    ('20260104', '赵六', '13800000003', '母亲', 'active'),
    ('20260105', '孙七', '13800000004', '父亲', 'inactive'),
    ('20260106', '周八', '13800000005', '母亲', 'active')
ON CONFLICT (student_no, parent_phone) DO NOTHING;

-- ============================================================================
-- 补充共享演示数据（覆盖各业务面；全部幂等，可重复执行）。
-- 覆盖：自定义场景 / 门禁白名单 / 运维日志 / AI 会话 / 订阅 / 家长绑定。
-- 说明：① 空间类型（space_types）已由 migration_v15 建表并种子，这里不再重复；
--       ② 内置「开启/离开」场景由网关懒种（scenes_seeded），这里只补自定义场景避免冲突。
-- ============================================================================

-- 自定义场景（kind=custom；内置 all_on/all_off 由网关懒种，不在此重复）。
INSERT INTO scenes (scene_id, space_id, name, kind, device_states) VALUES
    ('scn_seed_a101_class', 'spc_a8acdd5c', '上课模式', 'custom',
     '[{"device_id":"dev_c2936745","device_type":"zigbee","command":"on"},'
     '{"device_id":"dev_9f8e7d6c","device_type":"fuhe-screen","command":"on"},'
     '{"device_id":"dev_1a2b3c4d","device_type":"fuhe-board","command":"on"}]'),
    ('scn_seed_a101_break', 'spc_a8acdd5c', '课间模式', 'custom',
     '[{"device_id":"dev_c2936745","device_type":"zigbee","command":"off"},'
     '{"device_id":"dev_9f8e7d6c","device_type":"fuhe-screen","command":"off"}]'),
    ('scn_seed_a102_class', 'spc_std_a102', '上课模式', 'custom',
     '[{"device_id":"dev_aa11bb22","device_type":"zigbee","command":"on"},'
     '{"device_id":"dev_ee55ff66","device_type":"fuhe-screen","command":"on"},'
     '{"device_id":"dev_cc33dd44","device_type":"fuhe-board","command":"on"}]'),
    ('scn_seed_gym_event', 'spc_gym_1', '活动模式', 'custom',
     '[{"device_id":"dev_gym_light","device_type":"zigbee","command":"on"},'
     '{"device_id":"dev_gym_screen","device_type":"fuhe-screen","command":"on"}]'),
    ('scn_seed_lib_quiet', 'spc_lib_1', '静音模式', 'custom',
     '[{"device_id":"dev_lib_light","device_type":"zigbee","command":"off"}]')
ON CONFLICT (scene_id) DO NOTHING;

-- 门禁白名单（组合主键 space_id+device_id；标记后控制接口对门禁 off 需 confirm=true）。
INSERT INTO door_devices (space_id, device_id, label, marked_by) VALUES
    ('spc_a8acdd5c',  'dev_d0or_a101', '前门门禁',   'u_admin_1'),
    ('spc_std_a102',  'dev_d0or_a102', '前门门禁',   'u_admin_1'),
    ('spc_b7bee44d',  'dev_99aabbcc',  '后门门禁',   'u_admin_1'),
    ('spc_office_1',  'dev_off_door',  '办公室门禁', 'u_admin_1'),
    ('spc_gym_1',     'dev_gym_door',  '入口门禁',   'u_admin_1'),
    ('spc_lab_1',     'dev_lab_door',  '实验室门禁', 'u_admin_1'),
    ('spc_lib_1',     'dev_lib_door',  '图书馆门禁', 'u_admin_1'),
    ('spc_canteen_1', 'dev_ctn_door',  '入口门禁',   'u_admin_1')
ON CONFLICT (space_id, device_id) DO NOTHING;

-- 运维日志样例（管理端 operation-logs 页可见；id 显式指定保证幂等）。
INSERT INTO operation_logs (id, op_type, user_id, space_id, scene_id, success_count, failed_count, detail) VALUES
    (1,  'scene_execute', 'u_admin_1', 'spc_a8acdd5c', 'scn_seed_a101_class', 3, 0, '{"scene":"上课模式"}'),
    (2,  'alert_handle',  'u_admin_1', 'spc_a8acdd5c', '', 1, 0, '{"alert_id":"alt_1","remark":"已处理"}'),
    (3,  'door_mark',     'u_admin_1', 'spc_a8acdd5c', '', 1, 0, '{"device_id":"dev_d0or_a101"}'),
    (4,  'scene_execute', 'u_admin_1', 'spc_std_a102', 'scn_seed_a102_class', 3, 0, '{"scene":"上课模式"}'),
    (5,  'alert_handle',  'u_admin_2', 'spc_lab_1',    '', 1, 0, '{"alert_id":"alt_8","remark":"已复位"}'),
    (6,  'space_bind',    'u_admin_1', 'spc_lab_1',    '', 1, 0, '{"user_id":"u_teacher_1"}'),
    (7,  'door_mark',     'u_admin_1', 'spc_gym_1',    '', 1, 0, '{"device_id":"dev_gym_door"}'),
    (8,  'alert_handle',  'u_admin_1', 'spc_std_a102', '', 1, 0, '{"alert_id":"alt_6","remark":"正在排查"}'),
    (9,  'scene_execute', 'u_admin_1', 'spc_gym_1',    'scn_seed_gym_event', 2, 0, '{"scene":"活动模式"}'),
    (10, 'door_unmark',   'u_admin_1', 'spc_lib_1',    '', 1, 0, '{"device_id":"dev_lib_door"}'),
    (11, 'space_unbind',  'u_admin_1', 'spc_office_1', '', 1, 0, '{"user_id":"u_teacher_2"}'),
    (12, 'scene_execute', 'u_admin_1', 'spc_lib_1',    'scn_seed_lib_quiet', 1, 0, '{"scene":"静音模式"}')
ON CONFLICT (id) DO NOTHING;
SELECT setval(pg_get_serial_sequence('operation_logs', 'id'),
              GREATEST((SELECT COALESCE(MAX(id), 1) FROM operation_logs), 1));

-- AI 会话历史样例（AI 助手页可回读历史；conversation_id 固定保证幂等）。
INSERT INTO ai_conversations (conversation_id, user_id, title) VALUES
    ('conv_seed_demo',  'u_admin_1',   'A101 有哪些设备在线？'),
    ('conv_seed_demo2', 'u_teacher_1', '帮我把 A101 的灯打开'),
    ('conv_seed_demo3', 'u_admin_1',   '哪些教室有告警？'),
    ('conv_seed_demo4', 'u_teacher_2', '体育馆现在温度多少？')
ON CONFLICT (conversation_id) DO NOTHING;

INSERT INTO ai_messages (id, conversation_id, role, content) VALUES
    (1, 'conv_seed_demo',  'user',      'A101 有哪些设备在线？'),
    (2, 'conv_seed_demo',  'assistant', 'A101 教室当前在线设备：教室照明、电子班牌、综合屏、前门门禁、环境传感器，共 5 台，全部在线。'),
    (3, 'conv_seed_demo2', 'user',      '帮我把 A101 的灯打开'),
    (4, 'conv_seed_demo2', 'assistant', '已为你打开 A101 教室的照明。'),
    (5, 'conv_seed_demo3', 'user',      '哪些教室有告警？'),
    (6, 'conv_seed_demo3', 'assistant', '当前有告警的教室：A101（设备离线）、A102（设备离线）、体育馆（传感器超阈值）、学生食堂（外围传感器离线）。'),
    (7, 'conv_seed_demo4', 'user',      '体育馆现在温度多少？'),
    (8, 'conv_seed_demo4', 'assistant', '体育馆环境传感器当前离线，暂无实时温度数据。')
ON CONFLICT (id) DO NOTHING;
SELECT setval(pg_get_serial_sequence('ai_messages', 'id'),
              GREATEST((SELECT COALESCE(MAX(id), 1) FROM ai_messages), 1));

-- 订阅消息（template_id 即事件类型 device_offline/peripheral_offline/sensor_threshold/face_login_failed）。
INSERT INTO subscriptions (user_id, template_id, quota) VALUES
    ('u_admin_1',   'device_offline',     3),
    ('u_admin_1',   'peripheral_offline', 3),
    ('u_admin_1',   'sensor_threshold',   3),
    ('u_admin_1',   'face_login_failed',  3),
    ('u_teacher_1', 'device_offline',     3),
    ('u_teacher_1', 'sensor_threshold',   3),
    ('u_teacher_2', 'device_offline',     3),
    ('u_teacher_2', 'face_login_failed',  3)
ON CONFLICT (user_id, template_id) DO NOTHING;

-- 家长-公众号绑定（到校通知；已关注公众号的家长）。
INSERT INTO parent_student_bindings (student_no, parent_openid_oa, phone, reach_status) VALUES
    ('20260101', 'openid_parent_zhangsan_oa', '13800000000', 'active'),
    ('20260102', 'openid_parent_lisi_oa',     '13800000001', 'active'),
    ('20260103', 'openid_parent_wangwu_oa',   '13800000002', 'active'),
    ('20260104', 'openid_parent_zhaoliu_oa',  '13800000003', 'active'),
    ('20260106', 'openid_parent_zhouba_oa',   '13800000005', 'active')
ON CONFLICT (student_no, parent_openid_oa) DO NOTHING;
