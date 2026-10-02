#include "../traffic-plugin/pudding_case.h"
#include <cstdio>
#include <stdexcept>

static unsigned checks=0;
void check(bool ok) { ++checks; if(!ok)throw std::runtime_error("Pudding carousel integration assertion failed"); }
void print_utf8(const std::wstring& text) {
    int size=WideCharToMultiByte(CP_UTF8,0,text.c_str(),-1,nullptr,0,nullptr,nullptr);
    std::string bytes(static_cast<size_t>(size),'\0');
    WideCharToMultiByte(CP_UTF8,0,text.c_str(),-1,bytes.data(),size,nullptr,nullptr);
    puts(bytes.c_str());
}
int wmain(int argc,wchar_t** argv) {
    compact::SharedState state;
    state.battery({85,1000,0,1,false,false});
    state.controller({64,1000,0,1,false,false,0});
    pudding::Reading r;r.status=pudding::Status::ok;r.timestamp=1000;r.matches=1;
    r.api_version=3;r.protocol_version=4;r.batteries={pudding::Battery{100,false},pudding::Battery{100,false},pudding::Battery{84,false}};
    compact::update_pudding_case(state,r);auto s=state.read();
    check(s.earcase_registered && s.earcase.value==84 && s.earcase.charge==-1);
    check(compact::carousel(s,10000).device==compact::BatteryDevice::earcase);
    check(compact::battery_text(*compact::carousel(s,10000).sample,10000)==L"84");
    check(compact::tooltip(s,10000).find(L"耳机仓电量：84%")!=std::wstring::npos);
    check(compact::tooltip(s,10000).find(L"GAIA API 3 / transport 4")!=std::wstring::npos);
    check(compact::battery_value(s.battery,41001)==vxe::unavailable);
    check(compact::battery_value(s.controller,41001)==vxe::unavailable);
    check(compact::battery_value(s.earcase,91000)==84);
    check(compact::battery_value(s.earcase,91001)==vxe::unavailable);
    check(compact::battery_value(s.earcase,999)==vxe::unavailable);
    for(auto status:{pudding::Status::missing_case,pudding::Status::malformed,pudding::Status::timeout,pudding::Status::transport_error,pudding::Status::unsupported,pudding::Status::ambiguous}){
        r.status=status;compact::update_pudding_case(state,r);s=state.read();
        check(s.earcase.value==vxe::unavailable);
        check(compact::tooltip(s,1000).find(L"耳机仓电量：84%")==std::wstring::npos);
    }
    r.status=pudding::Status::offline;compact::update_pudding_case(state,r);
    check(compact::battery_text(state.read().earcase,1000)==L"--");
    r.status=pudding::Status::ok;r.batteries[2].percent=-1;compact::update_pudding_case(state,r);
    check(state.read().earcase.value==vxe::unavailable); // Ear100 must not replace missing case.
    r.batteries[2].percent=222;compact::update_pudding_case(state,r);
    check(state.read().earcase.value==vxe::unavailable);
    for(int value:{0,100}){r.batteries[2].percent=value;compact::update_pudding_case(state,r);check(state.read().earcase.value==value);}
    if(argc>1){
        auto dll=LoadLibraryW(argv[1]);check(dll!=nullptr);
        auto factory=reinterpret_cast<ITMPlugin*(*)()>(GetProcAddress(dll,"TMPluginGetInstance"));check(factory!=nullptr);
        auto plugin=factory();check(plugin->GetAPIVersion()==7);
        check(plugin->GetItem(2)==nullptr);
        check(std::wstring(plugin->GetItem(0)->GetItemId())==L"vxe_r1_battery");
        check(std::wstring(plugin->GetItem(1)->GetItemId())==L"vxe_compact_panel");
        check(std::wstring(plugin->GetInfo(ITMPlugin::TMI_VERSION))==L"0.5.0-pudding");
        FreeLibrary(dll); // Never start HID workers in a test competing with installed DLL.
    }
    printf("PASS: %u carousel adapter/TTL/ABI checks; no device calls\n",checks);
    if(argc>2 && wcscmp(argv[2],L"--collect")==0){
        auto reading=pudding::query();compact::update_pudding_case(state,reading);s=state.read();
        print_utf8(compact::tooltip(s,GetTickCount64()));
        if(reading.status!=pudding::Status::ok || s.earcase.value!=reading.batteries[2].percent)return 2;
        check(s.earcase.value>=0 && s.earcase.value<=100);
        auto phase=compact::carousel(s,10000);
        check(phase.device==compact::BatteryDevice::earcase && phase.sample==&s.earcase);
        printf("PASS: actual GAIA -> existing earcase state -> E3 carousel entry, case=%d; no HID queries\n",s.earcase.value);
    }
}
