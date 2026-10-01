#include "native_hid.h"
#include "plugin_items.h"
#include <atomic>
#include <array>
#include <chrono>
#include <string>
#include <thread>
class BatteryPlugin : public ITMPlugin {
    compact::SharedState state;
    MouseItem mouse{state};
    CompactItem panel{state};
    std::atomic<bool> started{false};
public:
    IPluginItem* GetItem(int index) override{
        return index==0 ? static_cast<IPluginItem*>(&mouse) : index==1 ? &panel : nullptr;
    }
    void OnMonitorInfo(const MonitorInfo& info) override{state.monitor(info);}
    void OnExtenedInfo(ExtendedInfoIndex index,const wchar_t* data) override{
        if(index==EI_VALUE_TEXT_COLOR && data){
            wchar_t* end=nullptr;auto value=wcstoul(data,&end,10);
            if(end!=data && *end==0 && value<=0xffffff)panel.color(static_cast<COLORREF>(value));
        }
    }
    void DataRequired() override{
        if(started.exchange(true))return;
        HMODULE module=nullptr;
        // No host unload callback: keep code and worker state for process life.
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&vxe::query),&module)){started=false;return;}
        try{
            std::thread([this]{for(;;){auto r=vxe::query();
                state.battery({r.value,GetTickCount64(),r.error,r.matches,r.timed_out,r.stop});
                if(r.stop)break;std::this_thread::sleep_for(std::chrono::seconds(30));}}).detach();
        }catch(...){started=false;}
    }
    const wchar_t* GetInfo(PluginInfoIndex index) override{
        switch(index){
        case TMI_NAME:return L"VXE R1 独立电量插件";
        case TMI_DESCRIPTION:return L"独立HID电量采集；可选网速/CPU/内存/鼠标电量双行极简面板。";
        case TMI_AUTHOR:return L"Local VXE Battery Project";
        case TMI_VERSION:return L"0.3.0";
        default:return L"";}
    }
    const wchar_t* GetTooltipInfo() override{
        thread_local std::wstring text;
        text=compact::tooltip(state.read(),GetTickCount64());return text.c_str();
    }
};
extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance(){static auto* plugin=new BatteryPlugin;return plugin;}
