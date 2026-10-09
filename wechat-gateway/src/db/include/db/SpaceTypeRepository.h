#pragma once

#include <algorithm>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// space_types 表的一行（见 sql/migration_v15.sql）。
// code 是唯一业务键，spaces.type 直接引用它。
struct SpaceType {
    std::string code;   // standard / lecture / office ...
    std::string name;   // 中文名
    int sort_order = 0;
    bool enabled = true;
    std::string icon;   // 预留图标名（默认空，UI v1 未启用）
};

class SpaceTypeRepository {
public:
    virtual ~SpaceTypeRepository() = default;

    virtual std::vector<SpaceType> listAll() const = 0;
    virtual std::optional<SpaceType> findByCode(const std::string& code) const = 0;

    // 新增/更新一行（code 为主键，upsert 覆盖）。
    virtual void upsert(const SpaceType& t) = 0;

    // 软删除（enabled=false）；不存在返回 false。
    virtual bool disable(const std::string& code) = 0;

    // 按给定 code 顺序重写 sort_order（0,1,2,...）；不在列表中的不动。
    virtual void setSortOrder(const std::vector<std::string>& codes) = 0;

    // 当前最大 sort_order（空表返回 -1）；POST 缺省 sort_order 时排最后用。
    virtual int maxSortOrder() const = 0;
};

// 内存 mock。在开发装配 / 测试中通过 add() 播种数据。
class InMemorySpaceTypeRepository : public SpaceTypeRepository {
public:
    void add(const SpaceType& t) { byCode_[t.code] = t; }

    std::vector<SpaceType> listAll() const override {
        std::vector<SpaceType> out;
        out.reserve(byCode_.size());
        for (const auto& [_, t] : byCode_) {
            out.push_back(t);
        }
        return out;
    }

    std::optional<SpaceType> findByCode(const std::string& code) const override {
        const auto it = byCode_.find(code);
        if (it == byCode_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    void upsert(const SpaceType& t) override { byCode_[t.code] = t; }

    bool disable(const std::string& code) override {
        auto it = byCode_.find(code);
        if (it == byCode_.end()) {
            return false;
        }
        it->second.enabled = false;
        return true;
    }

    void setSortOrder(const std::vector<std::string>& codes) override {
        int i = 0;
        for (const auto& code : codes) {
            auto it = byCode_.find(code);
            if (it != byCode_.end()) {
                it->second.sort_order = i;
            }
            ++i;
        }
    }

    int maxSortOrder() const override {
        int m = -1;
        for (const auto& [_, t] : byCode_) {
            m = std::max(m, t.sort_order);
        }
        return m;
    }

private:
    std::unordered_map<std::string, SpaceType> byCode_;
};
