#pragma once
#include <cstdint>
#include <optional>

namespace controller_battery {
// Protocol values from xinput.h; model stays platform-independent for tests.
enum class Quality { offline, unavailable, non_battery, unknown_type, ordinal };
struct Ordinal { Quality quality; int level = -1; };
inline Ordinal classify(std::uint32_t state_result, std::uint32_t battery_result,
                        unsigned type, unsigned level) noexcept {
    if (state_result == 1167) return {Quality::offline};
    if (state_result != 0 || battery_result != 0) return {Quality::unavailable};
    if (type == 0) return {Quality::unavailable};
    if (type == 1) return {Quality::non_battery};
    if (type != 2 && type != 3) return {Quality::unknown_type};
    if (level > 3) return {Quality::unavailable};
    return {Quality::ordinal, static_cast<int>(level)};
}
inline const char* name(Quality q) noexcept {
    switch(q) {
    case Quality::offline: return "offline";
    case Quality::non_battery: return "wired_placeholder_not_battery";
    case Quality::unknown_type: return "unknown_battery_type_not_validated";
    case Quality::ordinal: return "reported_ordinal_needs_hardware_comparison";
    default: return "unavailable";
    }
}
inline std::optional<int> percentage(bool battery_present, std::optional<int> remaining,
                                    std::optional<int> full) noexcept {
    if (!battery_present || !remaining || !full || *full <= 0 ||
        *remaining < 0 || *remaining > *full) return std::nullopt;
    return static_cast<int>((static_cast<std::int64_t>(*remaining)*100 + *full/2)/ *full);
}
}
