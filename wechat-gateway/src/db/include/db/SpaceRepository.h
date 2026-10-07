#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// spaces 表的一行（见 sql/migration_v1.sql）。
struct Space {
    std::string space_id;
    std::string name;
    std::string type;  // standard / lecture / office（空间类型，第一级分组）
};

// TODO: 使用 PgPool 实现基于 PostgreSQL 的仓库。
class SpaceRepository {
public:
    virtual ~SpaceRepository() = default;

    virtual std::vector<Space> listAll() const = 0;
    virtual std::optional<Space> findById(const std::string& space_id) const = 0;

    // 空间生命周期同步（7.7 收尾项 e）：upsert 一行（不存在则插，存在则更新
    // name/type）；remove 删除一行（幂等）。
    virtual void upsert(const Space& space) = 0;
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

    std::optional<Space> findById(const std::string& space_id) const override {
        const auto it = byId_.find(space_id);
        if (it == byId_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    void upsert(const Space& space) override { byId_[space.space_id] = space; }

    void remove(const std::string& space_id) override {
        byId_.erase(space_id);
    }

private:
    std::unordered_map<std::string, Space> byId_;
};
