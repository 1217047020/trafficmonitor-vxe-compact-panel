#pragma once
#include <cstdint>
#include <mutex>

namespace beitong {
constexpr int offline = -2;
constexpr std::uint64_t expiry_ms = 40000;
struct Sample {
    int percentage = -1, charge = -1;
    std::uint64_t completed = 0;
    unsigned error = 0, matches = 0;
    bool stopped = false;
};
inline int visible_value(const Sample& s, std::uint64_t now) {
    if (!s.completed || now < s.completed || now - s.completed > expiry_ms || s.stopped) return -1;
    return s.percentage >= 0 && s.percentage <= 100 ? s.percentage : s.percentage == offline ? offline : -1;
}
class SharedState {
    mutable std::mutex mutex;
    Sample sample;
public:
    Sample read() const { std::lock_guard<std::mutex> lock(mutex); return sample; }
    void write(const Sample& s) { std::lock_guard<std::mutex> lock(mutex); sample = s; }
};
}
