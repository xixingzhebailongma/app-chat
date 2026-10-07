#pragma once

#include <ctime>
#include <fstream>
#include <mutex>
#include <string>

#include <nlohmann/json.hpp>

#include "db/OperationLogRepository.h"

namespace oplog {

inline std::string nowIsoUtc() {
    const std::time_t t = std::time(nullptr);
    const std::tm* gmt = std::gmtime(&t);
    if (!gmt) {
        return "";
    }
    char buf[40];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", gmt);
    return buf;
}

// 写入运维日志（含文件兜底）：
//   - repo 非空且 insert() 成功 → 返回 true（DB 落库，不写文件）；
//   - repo 为空或 insert() 返回 false → 追加本地 JSONL 文件兜底，返回 false。
// 无论 PG 抖动（insert 返回 false）还是 PG 完全不可用（repo 为空），
// 都落文件，记录不丢。filePath 为空串 = 禁用文件兜底（仅返回 false）。
inline bool writeOperationLog(OperationLogRepository* repo,
                              const OperationLogEntry& e,
                              const std::string& filePath) {
    bool ok = false;
    if (repo) {
        ok = repo->insert(e);
    }
    if (ok) {
        return true;
    }
    if (filePath.empty()) {
        return false;
    }

    static std::mutex m;
    const nlohmann::json line{
        {"op_type", e.op_type},
        {"user_id", e.user_id},
        {"space_id", e.space_id},
        {"scene_id", e.scene_id},
        {"success_count", e.success_count},
        {"failed_count", e.failed_count},
        {"detail", nlohmann::json::parse(e.detail, nullptr, false)},
        {"created_at", nowIsoUtc()}};
    std::lock_guard<std::mutex> lock(m);
    std::ofstream f(filePath, std::ios::app);
    if (f.is_open()) {
        f << line.dump() << "\n";
    }
    return false;
}

}  // namespace oplog
