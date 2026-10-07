#pragma once

#include <string>
#include <vector>

#include "dto/NotifySendDto.h"

// 一次分发中的单渠道尝试（对应 notify_attempts 表中的一行）。
// is_fallback 标志表示主渠道失败后的短信兜底。
struct ChannelAttempt {
    std::string channel;
    bool success = false;
    bool is_fallback = false;
    std::string error_msg;
    std::string target_user;  // 按用户路由（设计文档 13.3）；"" = 事件级
    int errcode = 0;          // 渠道返回的错误码（0 = 无 / 成功）
};

// 持久化通知分发记录（notify_logs + notify_attempts）。
// TODO: 实现基于 PostgreSQL 的仓库（见 sql/migration_v1.sql）。
class NotifyLogRepository {
public:
    virtual ~NotifyLogRepository() = default;

    virtual void save(const std::string& notify_id,
                      const NotifySendRequest& req,
                      const std::vector<ChannelAttempt>& attempts,
                      const std::string& final_status) = 0;
};

class InMemoryNotifyLogRepository : public NotifyLogRepository {
public:
    struct Record {
        std::string notify_id;
        std::string event_type;
        std::string space_id;
        std::string device_id;
        std::vector<std::string> target_roles;
        bool space_teachers = false;
        std::vector<ChannelAttempt> attempts;
        std::string final_status;
    };

    void save(const std::string& notify_id,
              const NotifySendRequest& req,
              const std::vector<ChannelAttempt>& attempts,
              const std::string& final_status) override {
        records_.push_back(Record{notify_id,
                                  req.event_type,
                                  req.space_id,
                                  req.device_id,
                                  req.target.roles,
                                  req.target.space_teachers,
                                  attempts,
                                  final_status});
    }

    const std::vector<Record>& records() const { return records_; }

private:
    std::vector<Record> records_;
};
