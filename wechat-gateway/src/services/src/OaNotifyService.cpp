#include "services/OaNotifyService.h"

#include <cassert>
#include <utility>

#include "utils/HttpResponseUtil.h"

namespace {

// 事件级日志占位：本阶段只写一条事件级日志，不逐家长写（逐家长幂等预留）。
constexpr const char* kEventLevelOpenid = "__event_level__";

}  // namespace

OaNotifyService::OaNotifyService(
    std::shared_ptr<ParentBindingRepository> bindingRepo,
    std::shared_ptr<WechatOaChannel> oaChannel,
    std::shared_ptr<OaNotifyLogRepository> logRepo,
    bool notifyOncePerDay,
    bool notifyOnLeave)
    : bindingRepo_(std::move(bindingRepo)),
      oaChannel_(std::move(oaChannel)),
      logRepo_(std::move(logRepo)),
      notifyOncePerDay_(notifyOncePerDay),
      notifyOnLeave_(notifyOnLeave) {
    // DI 保证 logRepo_ 非空（main.cpp 两个分支均注入），否则 event_id 幂等
    // 会静默失效；debug 断言捕获，release 由 DI 保证非空。
    assert(logRepo_ && "OaNotifyService: logRepo_ must be non-null");
}

ArrivalNotifyResponse OaNotifyService::notify(const ArrivalNotifyRequest& req) {
    ArrivalNotifyResponse resp;

    // event_id 与 notify_id 统一：请求带 event_id 就用它，为空生成一个。
    const std::string eventId =
        req.event_id.empty() ? http_util::newId("arrival") : req.event_id;
    resp.notify_id = eventId;
    // 渠道名：有渠道取 name()，无渠道兜底 "none"（避免误读成走了微信）。
    resp.channel = oaChannel_ ? oaChannel_->name() : "none";

    // openid 缺省为事件级占位（skipped/no_parent_binding 等）；逐家长时传真实
    // openid，写 parent_openid=openid 的家长级日志（唯一键 (event_id, parent_openid)）。
    auto writeLog = [&](const std::string& status, const std::string& reason,
                        const std::string& openid = kEventLevelOpenid,
                        int errcode = 0) {
        if (!logRepo_) {
            return;
        }
        OaNotifyLogEntry e;
        e.event_id = eventId;
        e.parent_openid = openid;
        e.student_no = req.student_no;
        e.space_id = req.space_id;
        e.event_type = req.event_type;
        e.status = status;
        e.reason = reason;
        e.errcode = errcode;
        logRepo_->save(e);
    };

    // 技术幂等：同一 event_id 已处理过 → duplicate，不发送、不写日志（7.8 §5 第 3 步）。
    // 查库失败视为可重试错误（不继续发送），置 db_error 由控制器映射 5xx 让边侧重试。
    const EventIdCheck check = logRepo_->existsByEventId(eventId);
    if (check == EventIdCheck::Present) {
        resp.accepted = true;
        resp.final_status = "duplicate";
        resp.reason = "duplicate";
        return resp;
    }
    if (check == EventIdCheck::Error) {
        resp.accepted = false;
        resp.final_status = "db_error";  // 非终态 failed；可重试，不写日志
        resp.reason = "db_error";
        resp.db_error = true;
        return resp;
    }

    // 离校开关：event_type=leave 且未开启离校通知 → skipped（7.8 §5 第 4 步）。
    if (req.event_type == "leave" && !notifyOnLeave_) {
        resp.accepted = true;
        resp.final_status = "skipped";
        resp.reason = "leave_disabled";
        writeLog(resp.final_status, resp.reason);
        return resp;
    }

    // 业务去重：当日 (student_no, space_id) 已成功推过 → skipped；查询失败
    // fail-closed（db_error 可重试），与 existsByEventId 一致，保「至多推一次」。
    if (notifyOncePerDay_) {
        const DedupCheck dedup =
            logRepo_->hasSuccessToday(req.student_no, req.space_id);
        if (dedup == DedupCheck::Present) {
            resp.accepted = true;
            resp.final_status = "skipped";
            resp.reason = "already_notified_today";
            writeLog(resp.final_status, resp.reason);
            return resp;
        }
        if (dedup == DedupCheck::Error) {
            resp.accepted = false;
            resp.final_status = "db_error";
            resp.reason = "db_error";
            resp.db_error = true;
            return resp;
        }
    }

    if (!bindingRepo_) {
        resp.final_status = "failed";
        return resp;
    }

    const std::vector<std::string> openids =
        bindingRepo_->findParentOpenidsByStudentNo(req.student_no);
    if (openids.empty()) {
        // 无绑定家长：正常业务结果，非错误。
        resp.accepted = true;
        resp.final_status = "no_parent_binding";
        resp.reason = "no_parent_binding";
        writeLog(resp.final_status, resp.reason);
        return resp;
    }

    // 渠道未启用（对象缺失或 enabled=false）→ channel_disabled，区分于 mock。
    if (!oaChannel_ || !oaChannel_->enabled()) {
        resp.accepted = true;
        resp.final_status = "skipped";
        resp.reason = "channel_disabled";
        writeLog(resp.final_status, resp.reason);
        return resp;
    }

    // 渠道已启用但未配置真实模板（real 模式 + 空/tmpl_* 占位）→ 配置错误。
    // 不静默 mock、不假装成功：返回 503 + CHANNEL_NOT_CONFIGURED，让边侧/运维
    // 感知到「未上线/未配置」而非「已推送」。
    if (!oaChannel_->isConfigured()) {
        resp.accepted = false;
        resp.final_status = "failed";
        resp.reason = "not_configured";
        resp.config_error = true;
        writeLog(resp.final_status, resp.reason);
        return resp;
    }

    // 逐家长发送（OaPayload 独立负载，不经通用 ChannelPayload）。
    // 逐家长写 oa_notify_logs（parent_openid 用真实 openid）；事件级
    // （skipped/no_parent_binding 等）已在前面用占位 __event_level__ 落日志。
    bool anySuccess = false;
    std::string lastReason;
    for (const std::string& openid : openids) {
        OaPayload p;
        p.student_no = req.student_no;
        p.student_name = req.student_name;
        p.space_name = req.space_name;
        p.arrival_time = req.arrival_time;
        p.oa_openid = openid;

        const ChannelResult r = oaChannel_->send(p);
        anySuccess = anySuccess || r.success;
        if (!r.message.empty()) {
            lastReason = r.message;
        }

        // 可达性：取关/拒收后标记，后续推送据此跳过（43004 未关注 / 43101 拒收）。
        if (!r.success && (r.errcode == 43004 || r.errcode == 43101)) {
            const std::string status =
                (r.errcode == 43004) ? "unsubscribed" : "refused";
            if (bindingRepo_) {
                bindingRepo_->updateReachStatus(openid, req.student_no, status);
            }
        }

        writeLog(r.success ? "success" : "failed",
                 r.message.empty() ? "" : r.message, openid, r.errcode);
    }

    resp.accepted = true;
    if (anySuccess) {
        resp.final_status = "success";
        resp.reason = "";
    } else {
        resp.final_status = "failed";
        resp.reason = lastReason.empty() ? "send_failed" : lastReason;
    }
    return resp;
}
