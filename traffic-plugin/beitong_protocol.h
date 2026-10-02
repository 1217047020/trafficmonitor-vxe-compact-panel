#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cwctype>
#include <string>

// Isolated protocol model only; NOT wired into the production plugin yet.
// Verified with KP20D XInput receiver 20BC:5159 and official status parser.
namespace beitong {
constexpr int unavailable = -1;
using Packet = std::array<std::uint8_t, 64>;
constexpr Packet status_request() { return {0x02, 0x15}; }
struct Status {
    int percentage_candidate = unavailable;
    int charge_state_raw = unavailable;
};
inline Status parse_status(const std::uint8_t* bytes, std::size_t length) noexcept {
    if (!bytes || length != 64 || bytes[0] != 0x02 ||
        bytes[1] != 0x15 || bytes[2] > 100) return {};
    return {bytes[2], bytes[4] & 0x0f};
}
inline bool target_path(std::wstring path) {
    for (auto& c : path) c = static_cast<wchar_t>(std::towlower(c));
    return path.find(L"vid_20bc&pid_5159&mi_01&col01#") != std::wstring::npos;
}
constexpr bool target_caps(unsigned vid, unsigned pid, unsigned page,
                           unsigned usage, unsigned input, unsigned output,
                           unsigned feature) {
    return vid == 0x20BC && pid == 0x5159 && page == 0xFF && usage == 3 &&
        input == 64 && output == 64 && feature == 0;
}
}
