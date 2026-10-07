#pragma once

#include <ctime>
#include <map>
#include <string>

// 通知模板占位符渲染：把模板字符串里的 {key} 替换为 vars[key]。
// 未提供的键保留原样。同时提供 {time} 占位所需的当前时间字符串。
namespace tmpl {

inline std::string render(const std::string& tpl,
                          const std::map<std::string, std::string>& vars) {
    std::string out = tpl;
    for (const auto& [key, value] : vars) {
        const std::string token = "{" + key + "}";
        std::string::size_type pos = 0;
        while ((pos = out.find(token, pos)) != std::string::npos) {
            out.replace(pos, token.size(), value);
            pos += value.size();
        }
    }
    return out;
}

// "YYYY-MM-DD HH:MM"（UTC）。用于订阅消息/短信模板中的时间字段。
inline std::string nowUtc() {
    const std::time_t now = std::time(nullptr);
    std::tm t{};
#if defined(_WIN32)
    gmtime_s(&t, &now);
#else
    gmtime_r(&now, &t);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &t);
    return std::string(buf);
}

// 从 NotifySendRequest 的常用字段构建占位符变量表。
inline std::map<std::string, std::string> notifyVars(
    const std::string& content, const std::string& device_label,
    const std::string& space_id, const std::string& severity) {
    return {{"content", content},
            {"device_label", device_label},
            {"space_id", space_id},
            {"severity", severity},
            {"time", nowUtc()}};
}

}  // namespace tmpl
