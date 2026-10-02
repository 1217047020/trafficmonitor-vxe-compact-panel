#include "battery_query.h"
#pragma warning(push)
#pragma warning(disable:4100) // Unused parameters in the unmodified host interface.
#include "../traffic-plugin/vendor/PluginInterface.h"
#pragma warning(pop)
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

namespace {
class State {
    mutable std::mutex mutex;
    pudding::Reading reading;
public:
    pudding::Reading read()const {std::lock_guard<std::mutex> lock(mutex);return reading;}
    void update(const pudding::Reading& value) {std::lock_guard<std::mutex> lock(mutex);reading=value;}
};
class Item:public IPluginItem {
    State& state;size_t index;const wchar_t* name;const wchar_t* id;const wchar_t* label;
public:
    Item(State& shared,size_t battery,const wchar_t* title,const wchar_t* identifier,const wchar_t* prefix)
        :state(shared),index(battery),name(title),id(identifier),label(prefix){}
    const wchar_t* GetItemName()const override {return name;}
    const wchar_t* GetItemId()const override {return id;}
    const wchar_t* GetItemLableText()const override {return label;}
    const wchar_t* GetItemValueSampleText()const override {return L"不可用";}
    const wchar_t* GetItemValueText()const override {
        thread_local std::array<std::wstring,3> text;
        text[index]=pudding::display(state.read(),index,GetTickCount64());return text[index].c_str();
    }
};
class Plugin:public ITMPlugin {
    State state;
    Item charger{state,2,L"布丁耳机仓（开发预览）",L"pudding_case_battery",L"耳机仓 "};
    Item left{state,0,L"布丁左耳（开发预览）",L"pudding_left_battery",L"左耳 "};
    Item right{state,1,L"布丁右耳（开发预览）",L"pudding_right_battery",L"右耳 "};
    std::atomic<bool> started{false};
public:
    IPluginItem* GetItem(int index)override {
        switch(index){case 0:return &charger;case 1:return &left;case 2:return &right;default:return nullptr;}
    }
    void DataRequired()override {
        if(started.exchange(true))return;
        HMODULE module=nullptr;
        // The host has no worker shutdown callback. Pin only if collection starts.
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                reinterpret_cast<LPCWSTR>(&pudding::query),&module)){started=false;return;}
        try {
            std::thread([this] {
                for(;;) {
                    pudding::Reading reading;
                    try {reading=pudding::query();}
                    catch(...) {reading.status=pudding::Status::transport_error;reading.error=ERROR_GEN_FAILURE;reading.timestamp=GetTickCount64();}
                    state.update(reading);
                    // Unsupported firmware is not hammered with repeat requests.
                    if(reading.status==pudding::Status::unsupported || reading.status==pudding::Status::ambiguous)break;
                    std::this_thread::sleep_for(std::chrono::seconds(60));
                }
            }).detach();
        }catch(...) {started=false;}
    }
    const wchar_t* GetInfo(PluginInfoIndex index)override {
        switch(index) {
        case TMI_NAME:return L"MOONDROP Pudding 电量（开发预览）";
        case TMI_DESCRIPTION:return L"原生 GAIA 蓝牙采集左右耳与仓电量；异常、超时或过期不显示百分比。";
        case TMI_VERSION:return L"0.1.0-dev";
        case TMI_AUTHOR:return L"Local Battery Project";
        default:return L"";
        }
    }
    const wchar_t* GetTooltipInfo()override {
        thread_local std::wstring text;
        auto reading=state.read();
        text=L"MOONDROP Pudding 开发预览\n";
        text+=pudding::status_text(reading.status);
        text+=L"\n已通过两种真实电量与手机 App 对照；不显示模拟值。";
        text+=L"\n充电时仓编码尚未确认，异常值显示不可用。";
        if(reading.error)text+=L"\nError: "+std::to_wstring(reading.error);
        if(reading.api_version)text+=L"\nGAIA API: "+std::to_wstring(reading.api_version)+L" / transport: "+std::to_wstring(reading.protocol_version);
        if(reading.device_status>=0)text+=L"\nProtocol status: "+std::to_wstring(reading.device_status);
        return text.c_str();
    }
};
}
extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance() {
    static auto* plugin=new Plugin;return plugin;
}
