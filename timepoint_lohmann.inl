#pragma once
// timepoint_lohmann.inl
// JSON serialization support for std::chrono::system_clock::time_point using nlohmann::json.
// dbien
// 21MAR2024

#include <chrono>
#include <string>

namespace nlohmann {
using namespace __BIENUTIL_USE_NAMESPACE;

template<>
struct adl_serializer<std::chrono::system_clock::time_point> {
    static void to_json(json& j, const std::chrono::system_clock::time_point& tp) {
        auto tt = std::chrono::system_clock::to_time_t(tp);
        std::tm tm = *std::gmtime(&tt);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            tp.time_since_epoch() % std::chrono::seconds(1));
        
        char buffer[32];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &tm);
        std::string result(buffer);
        result += '.' + std::to_string(ms.count()) + 'Z';
        j = result;
    }

    static void from_json(const json& j, std::chrono::system_clock::time_point& tp) {
        std::string iso8601 = j.get<std::string>();
        std::tm tm = {};
        int milliseconds;
        sscanf(iso8601.c_str(), "%d-%d-%dT%d:%d:%d.%dZ",
               &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
               &tm.tm_hour, &tm.tm_min, &tm.tm_sec,
               &milliseconds);
        
        tm.tm_year -= 1900;
        tm.tm_mon -= 1;
        
        tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
        tp += std::chrono::milliseconds(milliseconds);
    }
};

} // namespace nlohmann 