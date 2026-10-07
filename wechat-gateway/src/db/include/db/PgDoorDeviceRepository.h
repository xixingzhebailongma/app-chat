#pragma once

#include <memory>
#include <string>
#include <vector>

#include "db/DoorDeviceRepository.h"

class PgPool;

// 基于 PostgreSQL 的 door_devices 仓库（sql/migration_v9.sql）。
// 仅在定义 HAS_LIBPQ 时编译（见 CMakeLists.txt）。
class PgDoorDeviceRepository : public DoorDeviceRepository {
public:
    explicit PgDoorDeviceRepository(std::shared_ptr<PgPool> pool);

    bool contains(const std::string& space_id,
                  const std::string& device_id) const override;
    std::vector<DoorDevice> list(const std::string& space_id) const override;
    bool add(const DoorDevice& d) override;
    bool remove(const std::string& space_id,
                const std::string& device_id) override;
    bool removeBySpace(const std::string& space_id) override;

private:
    std::shared_ptr<PgPool> pool_;
};
