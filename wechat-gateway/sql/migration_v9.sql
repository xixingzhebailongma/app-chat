-- 门禁白名单（7.5③ 开门二次确认）。显式标记「哪个 zigbee switch 是门禁」，
-- 因为真实边侧设备列表无 zb_role（见 docs/zb_role-调研与边侧需求.md）。
-- 组合主键 (space_id, device_id)：device_id 是 Zigbee 短地址（ZB_0x{4hex}），
-- 仅单个网关内唯一；space→gateway 1:1，故 (space_id, device_id) 才全局唯一。
CREATE TABLE IF NOT EXISTS door_devices (
    space_id   VARCHAR(64) NOT NULL,
    device_id  VARCHAR(64) NOT NULL,
    label      VARCHAR(128) NOT NULL DEFAULT '',
    marked_by  VARCHAR(64) NOT NULL DEFAULT '', -- 当前标记者快照；完整历史见 operation_logs
    marked_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (space_id, device_id)
);
