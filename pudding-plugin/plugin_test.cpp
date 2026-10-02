#define NOMINMAX
#include <windows.h>
#pragma warning(push)
#pragma warning(disable:4100)
#include "../traffic-plugin/vendor/PluginInterface.h"
#pragma warning(pop)
#include <cstdio>
#include <cwchar>
#include <cstdlib>
#include <string>
bool percentage(const wchar_t* text) {
    if(!text || *text<L'0' || *text>L'9')return false;
    wchar_t* end=nullptr;auto number=std::wcstol(text,&end,10);
    return number>=0 && number<=100 && end && *end==L'%' && end[1]==0;
}
void print_utf8(const wchar_t* text) {
    int size=WideCharToMultiByte(CP_UTF8,0,text,-1,nullptr,0,nullptr,nullptr);
    if(size<=0)return;
    std::string bytes(static_cast<size_t>(size),'\0');
    WideCharToMultiByte(CP_UTF8,0,text,-1,bytes.data(),size,nullptr,nullptr);
    puts(bytes.c_str());
}
int wmain(int argc,wchar_t** argv) {
    if(argc<2)return 2;
    auto dll=LoadLibraryW(argv[1]);if(!dll){printf("LoadLibrary error=%lu\n",GetLastError());return 3;}
    auto get=reinterpret_cast<ITMPlugin*(*)()>(GetProcAddress(dll,"TMPluginGetInstance"));
    if(!get)return 4;
    auto plugin=get();if(!plugin || plugin!=get() || plugin->GetAPIVersion()!=7)return 5;
    if(plugin->GetItem(-1) || plugin->GetItem(3))return 6;
    const wchar_t* ids[]={L"pudding_case_battery",L"pudding_left_battery",L"pudding_right_battery"};
    for(int i=0;i<3;++i) {
        auto item=plugin->GetItem(i);
        if(!item || wcscmp(item->GetItemId(),ids[i]) || wcscmp(item->GetItemValueText(),L"等待"))return 7;
        if(item->IsCustomDraw() || item->OnMouseEvent(IPluginItem::MT_RCLICKED,0,0,nullptr,0)!=0)return 8;
    }
    if(!wcsstr(plugin->GetInfo(ITMPlugin::TMI_VERSION),L"dev"))return 9;
    if(!wcsstr(plugin->GetTooltipInfo(),L"充电时仓编码尚未确认"))return 10;
    puts("PASS: DLL ABI, 3 stable item IDs, initial fail-closed state; no device access");
    if(argc>2 && wcscmp(argv[2],L"--collect")==0) {
        plugin->DataRequired();plugin->DataRequired();
        auto deadline=GetTickCount64()+22000;
        while(wcscmp(plugin->GetItem(0)->GetItemValueText(),L"等待")==0 && GetTickCount64()<deadline)Sleep(100);
        auto text=plugin->GetItem(0)->GetItemValueText();
        print_utf8((std::wstring(L"Live DLL case display: ")+text).c_str());
        print_utf8(plugin->GetTooltipInfo());
        if(wcscmp(text,L"等待")==0)return 11;
        if(argc>3 && wcscmp(argv[3],L"--expect-unavailable")==0 && wcspbrk(text,L"0123456789%"))return 12;
        if(argc>3 && wcscmp(argv[3],L"--expect-numeric")==0) {
            for(int i=0;i<3;++i) {
                auto value=plugin->GetItem(i)->GetItemValueText();
                if(!percentage(value))return 13;
                print_utf8((std::wstring(ids[i])+L": "+value).c_str());
            }
        }
        puts("PASS: background collection completed; requested display expectation passed");
    }
    // No worker in the default test. A collected DLL is process-lifetime pinned.
    FreeLibrary(dll);return 0;
}
