#pragma once
#include "vendor/PluginInterface.h"
#include "native_protocol.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace compact {
constexpr std::uint64_t expiry_ms = 40000;
struct BatterySample {
    int value = vxe::unavailable;
    std::uint64_t completed = 0;
    unsigned error = 0, matches = 0;
    bool timeout = false, stopped = false;
};
struct Snapshot {
    ITMPlugin::MonitorInfo monitor{};
    bool monitor_received = false;
    BatterySample battery;
};
class SharedState {
    mutable std::mutex mutex;
    Snapshot current;
public:
    Snapshot read() const { std::lock_guard<std::mutex> lock(mutex); return current; }
    void monitor(const ITMPlugin::MonitorInfo& info) {
        std::lock_guard<std::mutex> lock(mutex);
        current.monitor = info; current.monitor_received = true;
    }
    void battery(const BatterySample& sample) {
        std::lock_guard<std::mutex> lock(mutex); current.battery = sample;
    }
};
inline bool expired(const BatterySample& sample, std::uint64_t now) {
    return !sample.completed || now < sample.completed || now - sample.completed > expiry_ms;
}
inline int battery_value(const BatterySample& sample, std::uint64_t now) {
    if (expired(sample, now)) return vxe::unavailable;
    return sample.value >= 0 && sample.value <= 100 ? sample.value :
        sample.value == vxe::offline ? vxe::offline : vxe::unavailable;
}
inline std::wstring percentage(int value) {
    return value >= 0 && value <= 100 ? std::to_wstring(value) : L"?";
}
inline std::wstring battery_text(const BatterySample& sample, std::uint64_t now) {
    int value = battery_value(sample, now);
    return value == vxe::offline ? L"--" : percentage(value);
}
// All ULL inputs stay within five characters (including unit). No T/P units:
// exceptional >=1024 GiB/s uses two-significant-digit scientific G notation.
inline std::wstring speed(std::uint64_t bytes) {
    static constexpr wchar_t units[] = L"BKMG";
    if (bytes < 1024) return std::to_wstring(bytes) + L'B';
    int unit = 1;
    long double value = static_cast<long double>(bytes) / 1024;
    while (unit < 3 && value >= 1024) { value /= 1024; ++unit; }
    auto rounded = [](long double n) { return n < 100 ? std::floor(n * 10 + .5L) / 10 : std::floor(n + .5L); };
    value = rounded(value);
    if (unit < 3 && value >= 1024) { value = rounded(value / 1024); ++unit; }
    if (unit == 3 && value >= 1024) {
        int exponent = 0;
        while (value >= 100) { value /= 10; ++exponent; }
        auto mantissa = static_cast<unsigned>(std::floor(value + .5L));
        if (mantissa == 100) { mantissa = 10; ++exponent; }
        return std::to_wstring(mantissa) + L'e' + std::to_wstring(exponent) + L'G';
    }
    auto tenths = static_cast<unsigned>(std::floor(value * 10 + .5L));
    std::wstring text = std::to_wstring(tenths / 10);
    if (tenths % 10) text += L'.' + std::to_wstring(tenths % 10);
    return text + units[unit];
}
inline const std::vector<std::wstring>& speed_bounds() {
    static const auto all = [] {
        std::vector<std::wstring> result{L"?"};
        for (unsigned n = 0; n < 1024; ++n) result.push_back(std::to_wstring(n) + L'B');
        for (wchar_t unit : std::wstring(L"KMG")) {
            for (unsigned n = 0; n < 1024; ++n) result.push_back(std::to_wstring(n) + unit);
            for (unsigned n = 10; n < 1000; ++n) {
                if (n % 10) result.push_back(std::to_wstring(n / 10) + L'.' + std::to_wstring(n % 10) + unit);
            }
        }
        // ULL_MAX / GiB = 17,179,869,184; possible exponents 2..9.
        for (unsigned e = 2; e <= 9; ++e)
            for (unsigned n = 10; n < 100; ++n)
                result.push_back(std::to_wstring(n) + L'e' + std::to_wstring(e) + L'G');
        return result;
    }();
    return all;
}
inline std::wstring battery_reason(const BatterySample& sample, std::uint64_t now) {
    if (!sample.completed) return L"尚未完成首次采样";
    if (expired(sample, now)) return sample.stopped ? L"采集线程已停止；采样已过期（超过40秒），旧电量已丢弃" : L"采样已过期（超过40秒），旧电量已丢弃";
    if (sample.stopped) return L"I/O取消未完成，采集线程已停止";
    if (sample.value == vxe::offline) return sample.timeout ? L"接收器未收到鼠标回复（超时/鼠标关闭）" : L"接收器或鼠标已离线（设备断开）";
    if (sample.value >= 0 && sample.value <= 100) return L"正常，已校验HID回复";
    return L"不可用（设备访问失败、接口不唯一或回复无效），错误码=" + std::to_wstring(sample.error);
}
inline std::wstring full_speed(std::uint64_t bytes) {
    // Exact byte count, no rounding loss in hover text.
    return std::to_wstring(bytes) + L" B/s（字节/秒，1024进位）";
}
inline std::wstring tooltip(const Snapshot& sample, std::uint64_t now) {
    const auto& m = sample.monitor;
    int value = battery_value(sample.battery, now);
    std::wstring text = L"下载速度：" + (sample.monitor_received ? full_speed(m.down_speed) : L"不可用（等待宿主快照）");
    text += L"\n上传速度：" + (sample.monitor_received ? full_speed(m.up_speed) : L"不可用（等待宿主快照）");
    text += L"\nCPU占用：" + (sample.monitor_received && m.cpu_usage >= 0 && m.cpu_usage <= 100 ? percentage(m.cpu_usage) + L"%" : L"不可用");
    text += L"\n内存占用：" + (sample.monitor_received && m.memory_usage >= 0 && m.memory_usage <= 100 ? percentage(m.memory_usage) + L"%" : L"不可用");
    text += L"\n鼠标电量：" + (value >= 0 ? percentage(value) + L"%" : value == vxe::offline ? L"离线（--）" : L"不可用（?）");
    text += L"\n鼠标采样状态：" + battery_reason(sample.battery, now);
    if (sample.battery.completed && now >= sample.battery.completed)
        text += L"；距完成 " + std::to_wstring((now - sample.battery.completed) / 1000) + L" 秒";
    text += L"\nHID每30秒单次查询；40秒过期；不保留异常旧电量";
    return text;
}
}
