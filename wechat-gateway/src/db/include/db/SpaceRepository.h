#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// spaces 表的一行（见 sql/migration_v1.sql + migration_v16.sql）。
struct Space {
    std::string space_id;
    std::string name;
    std::string type;  // standard / lecture / office（空间类型，第一级分组）
    bool enabled = true;
    std::string source = "edge";      // edge（边侧同步）/ admin（管理员新建）
    std::string active_scene_id = "";  // 当前激活场景（自定义场景功能）
    bool scenes_seeded = false;        // 默认场景是否已种入
};

class SpaceRepository {
public:
    virtual ~SpaceRepository() = default;

    virtual std::vector<Space> listAll() const = 0;      // 全部（含停用）
    virtual std::vector<Space> listEnabled() const = 0;  // 仅启用（miniapp 读）
    virtual std::optional<Space> findById(const std::string& space_id) const = 0;

    // 全量 upsert（覆盖 name/type/enabled/source）。
    virtual void upsert(const Space& space) = 0;

    // 管理员编辑：改 name/type，并置 source=admin（接管）——后续 sync 不覆盖。
    virtual void updateMeta(const std::string& space_id,
                            const std::string& name,
                            const std::string& type) = 0;

    // 管理员设置启停（enabled）并置 source=admin（接管）——后续 sync 不覆盖/不重新启用。
    virtual void setEnabled(const std::string& space_id, bool enabled) = 0;

    // 场景激活态（自定义场景功能）：set 写入、clear 清空。
    virtual void setActiveScene(const std::string& space_id,
                                const std::string& scene_id) = 0;
    virtual void clearActiveScene(const std::string& space_id) = 0;
    // 默认场景种入标记。
    virtual void markScenesSeeded(const std::string& space_id) = 0;

    // 物理删除（edge sync 对账用）。
    virtual void remove(const std::string& space_id) = 0;
};

// 内存 mock。在开发装配 / 测试中通过 add() 播种数据。
class InMemorySpaceRepository : public SpaceRepository {
public:
    void add(const Space& space) { byId_[space.space_id] = space; }

    std::vector<Space> listAll() const override {
        std::vector<Space> out;
        out.reserve(byId_.size());
        for (const auto& [_, space] : byId_) {
            out.push_back(space);
        }
        return out;
    }

    std::vector<Space> listEnabled() const override {
        std::vector<Space> out;
        for (const auto& [_, space] : byId_) {
            if (space.enabled) {
                out.push_back(space);
            }
        }
        return out;
    }

    std::optional<Space> findById(const std::string& space_id) const override {
        const auto it = byId_.find(space_id);
        if (it == byId_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    void upsert(const Space& space) override { byId_[space.space_id] = space; }

    void updateMeta(const std::string& space_id, const std::string& name,
                    const std::string& type) override {
        auto it = byId_.find(space_id);
        if (it != byId_.end()) {
            it->second.name = name;
            it->second.type = type;
            it->second.source = "admin";
        }
    }

    void setEnabled(const std::string& space_id, bool enabled) override {
        auto it = byId_.find(space_id);
        if (it != byId_.end()) {
            it->second.enabled = enabled;
            it->second.source = "admin";
        }
    }

    void setActiveScene(const std::string& space_id,
                        const std::string& scene_id) override {
        auto it = byId_.find(space_id);
        if (it != byId_.end()) {
            it->second.active_scene_id = scene_id;
        }
    }

    void clearActiveScene(const std::string& space_id) override {
        auto it = byId_.find(space_id);
        if (it != byId_.end()) {
            it->second.active_scene_id = "";
        }
    }

    void markScenesSeeded(const std::string& space_id) override {
        auto it = byId_.find(space_id);
        if (it != byId_.end()) {
            it->second.scenes_seeded = true;
        }
    }

    void remove(const std::string& space_id) override {
        byId_.erase(space_id);
    }

private:
    std::unordered_map<std::string, Space> byId_;
};
