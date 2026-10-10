#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// scenes 表的一行（见 sql/migration_v17.sql）。场景挂空间（space_id），按教室共享。
struct Scene {
    std::string scene_id;
    std::string space_id;
    std::string name;
    std::string kind;           // custom / all_on / all_off
    std::string device_states;  // custom 用：JSON [{device_id,device_type,command}]
};

class SceneRepository {
public:
    virtual ~SceneRepository() = default;

    virtual std::vector<Scene> listBySpace(const std::string& space_id) const = 0;
    virtual std::optional<Scene> findById(const std::string& scene_id) const = 0;
    virtual void upsert(const Scene& scene) = 0;
    virtual void remove(const std::string& scene_id) = 0;
};

// 内存 mock。在开发装配 / 测试中通过 add() 播种数据。
class InMemorySceneRepository : public SceneRepository {
public:
    void add(const Scene& scene) { byId_[scene.scene_id] = scene; }

    std::vector<Scene> listBySpace(const std::string& space_id) const override {
        std::vector<Scene> out;
        for (const auto& [_, scene] : byId_) {
            if (scene.space_id == space_id) {
                out.push_back(scene);
            }
        }
        return out;
    }

    std::optional<Scene> findById(const std::string& scene_id) const override {
        const auto it = byId_.find(scene_id);
        if (it == byId_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    void upsert(const Scene& scene) override { byId_[scene.scene_id] = scene; }

    void remove(const std::string& scene_id) override {
        byId_.erase(scene_id);
    }

private:
    std::unordered_map<std::string, Scene> byId_;
};
