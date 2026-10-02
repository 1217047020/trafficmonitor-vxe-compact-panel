#include "pudding_case.h"
#include "native_hid.h"
#include "beitong_hid.h"
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
    std::atomic<bool> case_started{false};
    void CaseRequired(){
        if(case_started.exchange(true))return;
        HMODULE module=nullptr;
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&pudding::query),&module)){case_started=false;return;}
        try{
            std::thread([this]{
                for(;;){
                    pudding::Reading reading;
                    try{reading=pudding::query();}
                    catch(...){reading.status=pudding::Status::transport_error;reading.error=ERROR_GEN_FAILURE;reading.timestamp=GetTickCount64();}
                    compact::update_pudding_case(state,reading);
                    if(reading.status==pudding::Status::unsupported || reading.status==pudding::Status::ambiguous)break;
                    std::this_thread::sleep_for(std::chrono::seconds(60));
                }
            }).detach();
        }catch(...){case_started=false;}
    }
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
        CaseRequired(); // Separate worker: RFCOMM deadlines never block HID sampling.
        if(started.exchange(true))return;
        HMODULE module=nullptr;
        // No host unload callback: keep code and worker state for process life.
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&vxe::query),&module)){started=false;return;}
        try{
            std::thread([this]{
                bool mouse_active=true, controller_active=true;
                for(;;){
                    if(mouse_active){auto r=vxe::query();
                        state.battery({r.value,GetTickCount64(),r.error,r.matches,r.timed_out,r.stop});
                        mouse_active=!r.stop;
                    }
                    if(controller_active){auto r=beitong::query_status();
                        int value=r.status.percentage_candidate;
                        if(!r.matches && !r.error)value=vxe::offline;
                        state.controller({value,GetTickCount64(),r.error,r.matches,r.timed_out,r.stop,r.status.charge_state_raw});
                        controller_active=!r.stop;
                    }
                    if(!mouse_active && !controller_active)break;
                    std::this_thread::sleep_for(std::chrono::seconds(30));
                }
            }).detach();
        }catch(...){started=false;}
    }
    const wchar_t* GetInfo(PluginInfoIndex index) override{
        switch(index){
        case TMI_NAME:return L"VXE R1 独立电量插件";
        case TMI_DESCRIPTION:return L"原生HID鼠标/手柄与GAIA布丁耳机仓采集；M3/G5/E3图标电量轮播，保持9pt和原布局。";
        case TMI_AUTHOR:return L"Local VXE Battery Project";
        case TMI_VERSION:return L"0.5.0-pudding";
        default:return L"";}
    }
    const wchar_t* GetTooltipInfo() override{
        thread_local std::wstring text;
        text=compact::tooltip(state.read(),GetTickCount64());return text.c_str();
    }
};
extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance(){static auto* plugin=new BatteryPlugin;return plugin;}
