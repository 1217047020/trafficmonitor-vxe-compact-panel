#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <Xinput.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Gaming.Input.h>
#include <winrt/Windows.Devices.Power.h>
#include <winrt/Windows.System.Power.h>
#include <chrono>
#include <iostream>
#include <thread>
#include "controller_battery_model.h"
#include "../traffic-plugin/vendor/json.hpp"
using nlohmann::json;
using namespace controller_battery;
template<typename T> std::optional<int> optional_int(const T& ref) {
    return ref ? std::optional<int>(ref.Value()) : std::nullopt;
}
json nullable(std::optional<int> value) { return value ? json(*value) : json(nullptr); }
int main() {
    json out={{"readOnly",true},{"hardwareComparisonPassed",false},
              {"capturedAtUnixMs",std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::system_clock::now().time_since_epoch()).count()},
              {"xinput",json::array()},{"rawGameControllers",json::array()}};
    HMODULE module=LoadLibraryExW(L"xinput1_4.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(module) {
        auto getstate=reinterpret_cast<decltype(&XInputGetState)>(GetProcAddress(module,"XInputGetState"));
        auto getbattery=reinterpret_cast<decltype(&XInputGetBatteryInformation)>(GetProcAddress(module,"XInputGetBatteryInformation"));
        if(getstate && getbattery) for(DWORD slot=0;slot<4;++slot) {
            XINPUT_STATE state{}; XINPUT_BATTERY_INFORMATION battery{};
            auto sr=getstate(slot,&state);
            auto br=getbattery(slot,BATTERY_DEVTYPE_GAMEPAD,&battery);
            auto result=classify(sr,br,battery.BatteryType,battery.BatteryLevel);
            out["xinput"].push_back({{"slot",slot},{"stateResult",sr},{"batteryResult",br},
                {"rawBatteryType",br==0?json(battery.BatteryType):json(nullptr)},
                {"rawBatteryLevel",br==0?json(battery.BatteryLevel):json(nullptr)},
                {"quality",name(result.quality)},{"ordinal",result.level>=0?json(result.level):json(nullptr)},
                {"percentage",nullptr}});
        }
        else out["xinputError"]="required exports missing";
        FreeLibrary(module);
    } else out["xinputError"]=GetLastError();
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        using winrt::Windows::Gaming::Input::RawGameController;
        // Device discovery is asynchronous; bounded wait, no UI or pairing actions.
        for(int attempt=0;attempt<8 && RawGameController::RawGameControllers().Size()==0;++attempt)
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        unsigned enumerated=0;
        for(auto const& rc : RawGameController::RawGameControllers()) {
            ++enumerated;
            const auto vid=rc.HardwareVendorId(), pid=rc.HardwareProductId();
            // Only the KP20 receiver identities observed in the task.
            if((vid!=0x20bc && vid!=0x20dd) || pid!=0x5159) continue;
            json device={{"vid",vid},{"pid",pid},{"name",winrt::to_string(rc.DisplayName())},
                         {"percentageCandidate",nullptr},{"reportPresent",false}};
            try {
                auto report=rc.TryGetBatteryReport();
                if(report) {
                    device["reportPresent"]=true;
                    auto full=optional_int(report.FullChargeCapacityInMilliwattHours());
                    auto remaining=optional_int(report.RemainingCapacityInMilliwattHours());
                    const auto status=report.Status();
                    bool present=status!=winrt::Windows::System::Power::BatteryStatus::NotPresent;
                    device["status"]=static_cast<int>(status);
                    device["fullChargeCapacityMWh"]=nullable(full);
                    device["remainingCapacityMWh"]=nullable(remaining);
                    device["percentageCandidate"]=nullable(percentage(present,remaining,full));
                }
            } catch(winrt::hresult_error const& e) { device["reportError"]=static_cast<int>(e.code()); }
            out["rawGameControllers"].push_back(device);
        }
        out["rawControllerEnumeratedCount"]=enumerated;
    } catch(winrt::hresult_error const& e) { out["winrtError"]=static_cast<int>(e.code()); }
    catch(std::exception const& e) { out["winrtError"]=e.what(); }
    std::cout << out.dump(2) << '\n';
}
