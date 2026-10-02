#include "beitong_hid.h"
#include "beitong_item.h"
#include <atomic>
#include <chrono>
#include <thread>

// Separate candidate DLL: existing VXE DLL and compact layout are untouched.
class BeitongPlugin : public ITMPlugin {
    beitong::SharedState state;
    BeitongItem item{state};
    std::atomic<bool> started{false};
public:
    IPluginItem* GetItem(int index) override { return index == 0 ? &item : nullptr; }
    void DataRequired() override {
        if (started.exchange(true)) return;
        HMODULE module = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                               reinterpret_cast<LPCWSTR>(&beitong::query_status), &module)) {
            started = false; return;
        }
        try {
            std::thread([this] {
                for (;;) {
                    const auto r = beitong::query_status();
                    int value = r.status.percentage_candidate;
                    if (!r.matches && !r.error) value = beitong::offline;
                    state.write({value, r.status.charge_state_raw, GetTickCount64(), r.error, r.matches, r.stop});
                    if (r.stop) break;
                    std::this_thread::sleep_for(std::chrono::seconds(30));
                }
            }).detach();
        } catch (...) { started = false; }
    }
    const wchar_t* GetInfo(PluginInfoIndex index) override {
        switch (index) {
        case TMI_NAME: return L"北通鲲鹏20电量（验证版）";
        case TMI_DESCRIPTION: return L"20BC:5159接收器只读HID电量；无官方客户端或伴随进程依赖。";
        case TMI_AUTHOR: return L"Local Peripheral Battery Project";
        case TMI_VERSION: return L"0.1.0-preview";
        default: return L"";
        }
    }
    const wchar_t* GetTooltipInfo() override {
        thread_local std::wstring text;
        const auto sample = state.read();
        const auto value = beitong::visible_value(sample, GetTickCount64());
        text = L"北通鲲鹏20：";
        if (value >= 0) {
            text += std::to_wstring(value) + L"%（设备上报）";
            text += sample.charge == 0 ? L"，未充电" : sample.charge == 1 ? L"，正在充电" : L"，充电状态未知";
        } else if (value == beitong::offline) {
            text += L"离线或非已验证连接模式（20BC:5159不存在）";
        } else {
            text += L"不可用";
            if (sample.stopped) text += L"（I/O取消未完成，采集已停止）";
            text += L"；错误码=" + std::to_wstring(sample.error);
        }
        text += L"\n仅查询02 15；每30秒采样，40秒过期；不保留异常旧电量。";
        return text.c_str();
    }
};
extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance() {
    static auto* plugin = new BeitongPlugin;
    return plugin;
}
