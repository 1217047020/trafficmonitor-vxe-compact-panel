#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#include "vendor/PluginInterface.h"
#include "battery_state.h"
#include <atomic>
#include <array>
#include <chrono>
#include <string>
#include <thread>
#include <condition_variable>
#include <mutex>

// WebSocket handles do not accept WINHTTP_OPTION_RECEIVE_TIMEOUT. A bounded
// watchdog closes the handle to cancel a stalled synchronous receive instead.
struct BoundedSocket {
    HINTERNET h;
    std::mutex mutex;
    std::condition_variable cv;
    bool done = false, closed = false;
    std::thread watchdog;
    explicit BoundedSocket(HINTERNET value) : h(value) {
      try { watchdog = std::thread([this] {
        std::unique_lock<std::mutex> lock(mutex);
        if (!cv.wait_for(lock, std::chrono::seconds(3), [this] { return done; })) {
            WinHttpCloseHandle(h);
            closed = true;
        }
      }); } catch (...) { WinHttpCloseHandle(h); throw; }
    }
    ~BoundedSocket() {
        { std::lock_guard<std::mutex> lock(mutex); done = true; }
        cv.notify_one();
        watchdog.join();
        if (!closed) WinHttpCloseHandle(h);
    }
    BoundedSocket(const BoundedSocket&) = delete;
};

struct Internet {
    HINTERNET h = nullptr;
    explicit Internet(HINTERNET value) : h(value) {}
    ~Internet() { if (h) WinHttpCloseHandle(h); }
    Internet(const Internet&) = delete;
};
static std::int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}
static INTERNET_PORT rpc_port() {
    DWORD p = 19211, size = sizeof(p);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\BatteryMonitor\\BatteryMonitor",
                 L"RpcPort", RRF_RT_REG_DWORD, nullptr, &p, &size);
    return p >= 1024 && p <= 65535 ? static_cast<INTERNET_PORT>(p) : 19211;
}
// A read-only snapshot: no settings writes, refresh RPCs or HID access.
static int fetch() noexcept {
    try {
        Internet session(WinHttpOpen(L"VXE-Battery-Bridge/0.1", WINHTTP_ACCESS_TYPE_NO_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
        if (!session.h || !WinHttpSetTimeouts(session.h, 1200, 1200, 1200, 1200)) return battery::unavailable;
        Internet connection(WinHttpConnect(session.h, L"127.0.0.1", rpc_port(), 0));
        if (!connection.h) return battery::unavailable;
        Internet request(WinHttpOpenRequest(connection.h, L"GET", L"/", nullptr,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0));
        DWORD redirects = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        if (!request.h || !WinHttpSetOption(request.h, WINHTTP_OPTION_REDIRECT_POLICY, &redirects, sizeof(redirects)) ||
            !WinHttpSetOption(request.h, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0) ||
            !WinHttpSendRequest(request.h, WINHTTP_NO_ADDITIONAL_HEADERS, 0, nullptr, 0, 0, 0) ||
            !WinHttpReceiveResponse(request.h, nullptr)) return battery::unavailable;
        DWORD status = 0, size = sizeof(status);
        if (!WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                nullptr, &status, &size, nullptr) || status != 101) return battery::unavailable;
        auto upgraded = WinHttpWebSocketCompleteUpgrade(request.h, 0);
        if (!upgraded) return battery::unavailable;
        BoundedSocket socket(upgraded);
        std::string query = R"({"jsonrpc":"2.0","method":"devices.list","params":{},"id":1})";
        if (WinHttpWebSocketSend(socket.h, WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
                                query.data(), static_cast<DWORD>(query.size()))) return battery::unavailable;
        std::string message;
        auto deadline = GetTickCount64() + 3000;
        for (unsigned fragments = 0; fragments < 32 && GetTickCount64() < deadline; ++fragments) {
            char buf[4096]; DWORD bytes = 0; WINHTTP_WEB_SOCKET_BUFFER_TYPE type;
            if (WinHttpWebSocketReceive(socket.h, buf, sizeof(buf), &bytes, &type)) return battery::unavailable;
            if (type != WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE &&
                type != WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE) return battery::unavailable;
            if (message.size() + bytes > 65536) return battery::unavailable;
            message.append(buf, bytes);
            if (type == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE) return battery::parse(message, now_ms());
        }
    } catch (...) {}
    return battery::unavailable;
}

class MouseItem : public IPluginItem {
public:
    std::atomic<int> value{battery::unavailable};
    std::atomic<ULONGLONG> received{0};
    std::array<std::wstring, 101> texts;
    MouseItem() { for (int i = 0; i <= 100; ++i) texts[i] = std::to_wstring(i) + L"%"; }
    const wchar_t* GetItemName() const override { return L"VXE R1 电量"; }
    const wchar_t* GetItemId() const override { return L"vxe_r1_battery"; }
    const wchar_t* GetItemLableText() const override { return L"鼠标 "; }
    const wchar_t* GetItemValueText() const override {
        // Monotonic TTL also catches stalled worker / sleep / stopped host polling.
        if (GetTickCount64() - received.load() > 10000) return L"不可用";
        int p = value.load();
        return p >= 0 && p <= 100 ? texts[p].c_str() : p == battery::offline ? L"离线" : L"不可用";
    }
    const wchar_t* GetItemValueSampleText() const override { return L"不可用"; }
};

class BatteryPlugin : public ITMPlugin {
    MouseItem mouse;
    std::atomic<bool> started{false};
public:
    IPluginItem* GetItem(int index) override { return index == 0 ? &mouse : nullptr; }
    void DataRequired() override {
        if (started.exchange(true)) return;
        // Host has no unload callback. Pin this DLL and keep the tiny instance for
        // the process lifetime, so a detached worker can never run unmapped code.
        HMODULE module = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                               reinterpret_cast<LPCWSTR>(&fetch), &module)) { started = false; return; }
        try {
            std::thread([this] {
                for (;;) {
                    mouse.value.store(fetch());
                    mouse.received.store(GetTickCount64());
                    std::this_thread::sleep_for(std::chrono::seconds(3));
                }
            }).detach();
        } catch (...) { started = false; }
    }
    const wchar_t* GetInfo(PluginInfoIndex index) override {
        switch (index) {
        case TMI_NAME: return L"VXE R1 电量桥接";
        case TMI_DESCRIPTION: return L"只读 BatteryMonitor 本机 WebSocket；不直接访问鼠标。";
        case TMI_AUTHOR: return L"Local VXE Battery Project";
        case TMI_VERSION: return L"0.1.0";
        default: return L"";
        }
    }
    const wchar_t* GetTooltipInfo() override { return L"VXE R1 星闪版\n数据源：BatteryMonitor\n离线或读取失败不保留旧百分比"; }
};
extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance() {
    static auto* plugin = new BatteryPlugin;
    return plugin;
}
