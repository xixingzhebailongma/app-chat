#pragma once

#include <memory>
#include <string>
#include <vector>

#include "db/NotifyLogRepository.h"

class PgPool;

// 基于 PostgreSQL 的 notify_logs + notify_attempts 仓库
// （sql/migration_v1.sql + migration_v3.sql）。save 在单个事务里写
// 主表与每条渠道尝试，保证一次通知要么完整落库、要么都不落库。
// 仅在定义 HAS_LIBPQ 时编译（见 CMakeLists.txt）。
class PgNotifyLogRepository : public NotifyLogRepository {
public:
    explicit PgNotifyLogRepository(std::shared_ptr<PgPool> pool);

    void save(const std::string& notify_id,
              const NotifySendRequest& req,
              const std::vector<ChannelAttempt>& attempts,
              const std::string& final_status) override;

private:
    std::shared_ptr<PgPool> pool_;
};
