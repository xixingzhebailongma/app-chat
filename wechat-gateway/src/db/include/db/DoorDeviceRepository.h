#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

// door_devices 表的一行（见 sql/migration_v9.sql）。显式标记「哪个 zigbee switch
// 是门禁」——真实边侧设备列表无 zb_role，故用白名单把「猜」变成「配置」。
struct DoorDevice {
    std::string space_id;
    std::string device_id;   // ZB_0x{short_addr}（真实）/ dev_...（mock）
    std::string label;       // 标记时快照，便于展示
    std::string marked_by;   // 当前标记者（快照）；完整历史见 operation_logs
    std::string marked_at;   // ISO8601
};

// 门禁白名单仓库。Pg 实现 + InMemory 实现（开发/测试）。
// 注意：必须只创建一个实例（main.cpp 用同一个 shared_ptr 分别注入
// DoorDeviceService 与 DeviceControlService），否则 InMemory 模式下 mark 与
// contains 不是同一份数据，428 永不触发（PG 模式会掩盖此 bug）。
class DoorDeviceRepository {
public:
    virtual ~DoorDeviceRepository() = default;

    // 是否已标记为门禁。热路径（control 每次控制调一次）；PG 模式为一次
    // 查询，如有性能问题改为内存缓存（本次不做）。
    virtual bool contains(const std::string& space_id,
                          const std::string& device_id) const = 0;

    // 列出门禁；space_id 空 = 全部，按 (space_id, device_id) 排序。
    virtual std::vector<DoorDevice> list(const std::string& space_id) const = 0;

    // upsert 覆盖（重复标记刷新 label/marked_by/marked_at）。
    virtual bool add(const DoorDevice& d) = 0;

    // 取消标记；幂等（不存在也算成功）。
    virtual bool remove(const std::string& space_id,
                        const std::string& device_id) = 0;

    // 删除某空间下的全部门禁标记（空间生命周期同步的级联清理；幂等）。
    virtual bool removeBySpace(const std::string& space_id) = 0;
};

// 内存 mock。组合键用 std::pair 映射（与 Pg 的 (space_id, device_id) 一致）。
class InMemoryDoorDeviceRepository : public DoorDeviceRepository {
public:
    bool contains(const std::string& space_id,
                  const std::string& device_id) const override {
        return byKey_.count({space_id, device_id}) > 0;
    }

    std::vector<DoorDevice> list(const std::string& space_id) const override {
        std::vector<DoorDevice> out;
        for (const auto& [k, d] : byKey_) {
            if (!space_id.empty() && k.first != space_id) {
                continue;
            }
            out.push_back(d);
        }
        return out;  // std::map 已按 key 升序
    }

    bool add(const DoorDevice& d) override {
        byKey_[{d.space_id, d.device_id}] = d;  // 覆盖
        return true;
    }

    bool remove(const std::string& space_id,
                const std::string& device_id) override {
        byKey_.erase({space_id, device_id});
        return true;
    }

    bool removeBySpace(const std::string& space_id) override {
        for (auto it = byKey_.begin(); it != byKey_.end();) {
            if (it->first.first == space_id) {
                it = byKey_.erase(it);
            } else {
                ++it;
            }
        }
        return true;
    }

private:
    std::map<std::pair<std::string, std::string>, DoorDevice> byKey_;
};
