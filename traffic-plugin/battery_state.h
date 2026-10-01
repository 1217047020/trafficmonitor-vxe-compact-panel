#pragma once
#include "vendor/json.hpp"
#include <cstdint>
#include <string>

namespace battery {
constexpr int unavailable = -1, offline = -2;
constexpr const char* device_id = "js:vxe/vxe-r1-nearlink:373B:10C9";
// Only accept the exact device, a fresh sample and a valid JSON-RPC response.
inline int parse(const std::string& text, std::int64_t now) noexcept {
    try {
        auto j = nlohmann::json::parse(text);
        if (j.at("jsonrpc") != "2.0" || j.at("id") != 1 || j.contains("error")) return unavailable;
        const auto& list = j.at("result").at("devices");
        if (!list.is_array()) return unavailable;
        const nlohmann::json* target = nullptr;
        for (const auto& d : list) {
            if (d.value("id", std::string()) == device_id) {
                if (target) return unavailable;
                target = &d;
            }
        }
        if (!target) return offline;
        const auto& d = *target;
        if (!d.at("connected").is_boolean() || !d.at("stale").is_boolean()) return unavailable;
        if (!d.at("connected").get<bool>() || d.at("stale").get<bool>()) return offline;
        if (!d.at("lastSeen").is_number_integer()) return unavailable;
        auto seen = d.at("lastSeen").get<std::int64_t>();
        // Verified v1.4.260908 runtime returns lastSeen=0 for this JS device.
        // Accept only a fresh server snapshot in that specific sentinel case.
        if (seen == 0) {
            const auto& updated = j.at("result").at("updatedAt");
            if (!updated.is_number_integer()) return unavailable;
            seen = updated.get<std::int64_t>();
        }
        if (seen <= 0 || seen > now + 5000 || now - seen > 90000) return unavailable;
        if (!d.at("percentage").is_number_integer()) return unavailable;
        auto p = d.at("percentage").get<std::int64_t>();
        return p >= 0 && p <= 100 ? static_cast<int>(p) : unavailable;
    } catch (...) { return unavailable; }
}
}
