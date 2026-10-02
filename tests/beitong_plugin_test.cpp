#define NOMINMAX
#include "../traffic-plugin/beitong_item.h"
#include <iostream>
#include <stdexcept>
void check(bool condition) { if (!condition) throw std::runtime_error("Beitong plugin assertion failed"); }
int wmain(int argc, wchar_t** argv) {
    using namespace beitong;
    check(visible_value({}, 1000) == -1);
    Sample s{63, 0, 1000, 0, 1, false};
    check(visible_value(s, 1000) == 63);
    check(visible_value(s, 41000) == 63);
    check(visible_value(s, 41001) == -1);
    check(visible_value(s, 999) == -1);
    s.percentage = 101; check(visible_value(s, 1000) == -1);
    s.percentage = offline; check(visible_value(s, 1000) == offline);
    s.percentage = 63; s.stopped = true; check(visible_value(s, 1000) == -1);
    SharedState state; BeitongItem item{state};
    check(std::wstring(item.GetItemId()) == L"beitong_kp20_battery");
    check(std::wstring(item.GetItemValueText()) == L"不可用");
    state.write({63, 0, GetTickCount64(), 0, 1, false});
    check(std::wstring(item.GetItemValueText()) == L"63%");
    state.write({offline, -1, GetTickCount64(), 0, 0, false});
    check(std::wstring(item.GetItemValueText()) == L"离线");
    if (argc == 2) {
        auto dll = LoadLibraryW(argv[1]); check(dll != nullptr);
        auto factory = reinterpret_cast<ITMPlugin*(*)()>(GetProcAddress(dll, "TMPluginGetInstance"));
        check(factory != nullptr); auto plugin = factory();
        check(plugin->GetAPIVersion() == 7);
        check(plugin->GetItem(-1) == nullptr && plugin->GetItem(1) == nullptr);
        check(std::wstring(plugin->GetItem(0)->GetItemId()) == L"beitong_kp20_battery");
        check(std::wstring(plugin->GetItem(0)->GetItemValueText()) == L"不可用");
        // ABI smoke only: NEVER call DataRequired / hardware from this test.
        FreeLibrary(dll);
    }
    std::cout << "Beitong state/item/ABI tests passed (no device access)\n";
}
