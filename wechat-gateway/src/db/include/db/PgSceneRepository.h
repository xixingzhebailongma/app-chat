#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "db/SceneRepository.h"

class PgPool;

// 基于 PostgreSQL 的 scenes 仓库（sql/migration_v17.sql）。
// 仅在定义 HAS_LIBPQ 时编译（见 CMakeLists.txt）。
class PgSceneRepository : public SceneRepository {
public:
    explicit PgSceneRepository(std::shared_ptr<PgPool> pool);

    std::vector<Scene> listBySpace(const std::string& space_id) const override;
    std::optional<Scene> findById(const std::string& scene_id) const override;
    void upsert(const Scene& scene) override;
    void remove(const std::string& scene_id) override;

private:
    std::shared_ptr<PgPool> pool_;
};
