#include "plugin_items.h"
#include <objidl.h>
#include <gdiplus.h>
#include <tlhelp32.h>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <limits>
#include <set>
namespace fs=std::filesystem;
using namespace Gdiplus;
static unsigned host_pid=0, host_dpi=96;
static int live_width=0,live_height=0;
static std::set<HWND> visited;
BOOL CALLBACK inspect(HWND wnd,LPARAM){
    DWORD pid=0;GetWindowThreadProcessId(wnd,&pid);
    if(pid==host_pid && !visited.count(wnd)){
        visited.insert(wnd);wchar_t name[256]{};GetClassNameW(wnd,name,256);
        wchar_t title[256]{};GetWindowTextW(wnd,title,256);
        RECT client{};GetClientRect(wnd,&client);
        char class_utf8[1024]{},title_utf8[1024]{};
        WideCharToMultiByte(CP_UTF8,0,name,-1,class_utf8,1024,nullptr,nullptr);
        WideCharToMultiByte(CP_UTF8,0,title,-1,title_utf8,1024,nullptr,nullptr);
        std::cout<<"host window: "<<class_utf8<<" title="<<title_utf8<<" client="<<client.right<<"x"<<client.bottom<<" dpi="<<GetDpiForWindow(wnd)<<" visible="<<IsWindowVisible(wnd)<<"\n";
        if(wcscmp(title,L"TrafficMonitorTaskbarWindow")==0){
            RECT r{};GetClientRect(wnd,&r);
            if(r.right>0 && r.bottom>0){live_width=r.right;live_height=r.bottom;host_dpi=GetDpiForWindow(wnd);}
        }
    }
    return TRUE;
}
BOOL CALLBACK inspect_top(HWND wnd,LPARAM arg){inspect(wnd,arg);EnumChildWindows(wnd,inspect,arg);return TRUE;}
struct Surface {
    HDC dc;HBITMAP bitmap;HGDIOBJ old_bitmap;int width,height;
    Surface(int w,int h):width(w),height(h){
        dc=CreateCompatibleDC(nullptr);BITMAPINFO info{};
        info.bmiHeader={sizeof(BITMAPINFOHEADER),w,-h,1,32,BI_RGB};
        void* pixels=nullptr;bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);old_bitmap=SelectObject(dc,bitmap);
    }
    ~Surface(){SelectObject(dc,old_bitmap);DeleteObject(bitmap);DeleteDC(dc);}
    void fill(COLORREF color){auto brush=CreateSolidBrush(color);RECT rect{0,0,width,height};FillRect(dc,&rect,brush);DeleteObject(brush);}
    void save(const fs::path& path){GdiFlush();Bitmap image(bitmap,nullptr);CLSID png{0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};if(image.Save(path.c_str(),&png)!=Ok)throw std::runtime_error("PNG save failed");}
};
HFONT make_font(int dpi){return CreateFontW(-MulDiv(9,dpi,72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH|FF_SWISS,L"微软雅黑");}
int legacy_width(HDC dc){
    // Exact V1.86 CalculateWindowSize rule using current config's 5 items,
    // four digits, non-short mode, spaces and 8-DIP item spacing.
    auto net=compact::text_width(dc,L"↑: ")+compact::text_width(dc,L"8888.8MB/s ");
    auto usage=std::max(compact::text_width(dc,L"CPU: "),compact::text_width(dc,L"内存: "))+compact::text_width(dc,L"100 %");
    auto battery=std::max(compact::text_width(dc,L"鼠标 "),compact::text_width(dc,L"不可用"));
    LOGFONTW lf{};GetObjectW(GetCurrentObject(dc,OBJ_FONT),sizeof(lf),&lf);
    return net+usage+battery+MulDiv(32,-lf.lfHeight,12);
}
void legacy_draw(HDC dc,int dpi,bool dark,int width,int height){
    auto color=dark?RGB(240,240,240):RGB(30,30,30);SetTextColor(dc,color);SetBkMode(dc,TRANSPARENT);
    int gap=MulDiv(8,dpi,96), net=compact::text_width(dc,L"↑: ")+compact::text_width(dc,L"8888.8MB/s ");
    int usage=std::max(compact::text_width(dc,L"CPU: "),compact::text_width(dc,L"内存: "))+compact::text_width(dc,L"100 %");
    int label_net=compact::text_width(dc,L"↑: "), label_usage=std::max(compact::text_width(dc,L"CPU: "),compact::text_width(dc,L"内存: "));
    auto draw=[&](const wchar_t* text,int l,int t,int r,int b,bool right){RECT rect{l,t,r,b};DrawTextW(dc,text,-1,&rect,DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX|(right?DT_RIGHT:DT_LEFT));};
    int first=gap, second=first+net+gap, third=second+usage+gap;
    draw(L"↑: ",first,0,first+label_net,height/2-1,false);draw(L"820 KB/s",first+label_net,0,first+net,height/2-1,true);
    draw(L"↓: ",first,height/2,first+label_net,height,false);draw(L"12.3 MB/s",first+label_net,height/2,first+net,height,true);
    draw(L"CPU: ",second,0,second+label_usage,height/2-1,false);draw(L"24 %",second+label_usage,0,second+usage,height/2-1,true);
    draw(L"内存: ",second,height/2,second+label_usage,height,false);draw(L"62 %",second+label_usage,height/2,second+usage,height,true);
    // Odd final host item: label above, value below (not a single wide row).
    RECT label_rect{third,0,width-gap,height/2},value_rect{third,height/2,width-gap,height};
    DrawTextW(dc,L"鼠标 ",-1,&label_rect,DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX|DT_CENTER);
    DrawTextW(dc,L"85%",-1,&value_rect,DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX|DT_CENTER);
}
int wmain(int argc,wchar_t** argv){
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if(argc<2)return 2;fs::path out=argv[1];fs::create_directories(out);
    GdiplusStartupInput startup;ULONG_PTR token;GdiplusStartup(&token,&startup,nullptr);
    auto process_snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);
    if(Process32FirstW(process_snapshot,&entry))do{if(_wcsicmp(entry.szExeFile,L"TrafficMonitor.exe")==0)host_pid=entry.th32ProcessID;}while(Process32NextW(process_snapshot,&entry));
    CloseHandle(process_snapshot);EnumWindows(inspect_top,0);
    compact::SharedState state;CompactItem panel(state);
    ITMPlugin::MonitorInfo monitor{820*1024,123*1024*1024/10,24,62};state.monitor(monitor);
    std::ofstream metrics(out/L"metrics.json");
    metrics << "{\n  \"host_pid\":"<<host_pid<<",\"host_dpi\":"<<host_dpi<<",\"live_client_width_px\":"<<live_width<<",\"live_client_height_px\":"<<live_height<<",\n  \"font\":\"Microsoft YaHei 9pt DEFAULT_QUALITY\",\"data_source\":\"synthetic snapshots; no HID query\",\n  \"scales\":[\n";
    int number=0;
    std::set<int> dpis{96,120,144,192};dpis.insert(host_dpi);
    for(int dpi:dpis){
        HDC dc=CreateCompatibleDC(nullptr);auto font=make_font(dpi);auto old=SelectObject(dc,font);
        auto layout=compact::measure(dc);int width=panel.GetItemWidthEx(dc),height=MulDiv(32,dpi,96),before=legacy_width(dc),after=width+MulDiv(16,dpi,96);
        if(number++)metrics << ",\n";
        metrics << "    {\"dpi\":"<<dpi<<",\"panel_width_px\":"<<width<<",\"panel_height_px\":"<<height<<",\"text_height_px\":"<<layout.text_height<<",\"panel_width_dip\":"<<width*96.0/dpi<<",\"old_host_width_px\":"<<before<<",\"new_host_width_px\":"<<after<<",\"host_reduction_percent\":"<<100.0*(1.0-double(after)/before)<<",\"cpu_rect_px\":["<<layout.usage_start()<<",0,"<<layout.usage_start()+layout.icon<<","<<height/2<<"]}";
        SelectObject(dc,old);DeleteDC(dc);
        for(bool dark:{false,true}){
            auto normal=dark?RGB(240,240,240):RGB(30,30,30),back=dark?RGB(32,32,32):RGB(242,242,242);
            panel.color(normal);std::wstring theme=dark?L"dark":L"light";
            if(argc>2 && wcscmp(argv[2],L"--carousel")==0){
                compact::Snapshot snap;snap.monitor_received=true;snap.monitor=monitor;
                snap.battery={85,30000,0,1,false,false};
                snap.controller={64,30000,0,1,false,false,0};snap.controller_registered=true;
                snap.earcase={94,30000,0,1,false,false};snap.earcase_registered=true;
                unsigned phase=0;
                for(const auto& device:{L"mouse",L"controller",L"case-mock"}){
                    Surface image(width,height);auto oldfont=SelectObject(image.dc,font);image.fill(back);
                    compact::draw(image.dc,0,0,width,height,dark,normal,layout,snap,30000+5000*phase++);
                    image.save(out/(theme+L"-"+std::to_wstring(dpi)+L"-carousel-"+device+L".png"));
                    SelectObject(image.dc,oldfont);
                }
                continue;
            }
            const std::vector<std::pair<std::wstring,compact::BatterySample>> states{
                {L"normal",{85,GetTickCount64(),0,1,false,false}},
                {L"zero",{0,GetTickCount64(),0,1,false,false}},
                {L"100",{100,GetTickCount64(),0,1,false,false}},
                {L"low20",{20,GetTickCount64(),0,1,false,false}},
                {L"low10",{10,GetTickCount64(),0,1,false,false}},
                {L"offline",{vxe::offline,GetTickCount64(),ERROR_TIMEOUT,1,true,false}},
                {L"expired",{85,GetTickCount64()-40001,0,1,false,false}},
                {L"unavailable",{vxe::unavailable,GetTickCount64(),ERROR_ACCESS_DENIED,1,false,false}},
                {L"K-M",{85,GetTickCount64(),0,1,false,false}},
                {L"M-G",{85,GetTickCount64(),0,1,false,false}},
                {L"huge",{100,GetTickCount64(),0,1,false,false}}
            };
            for(const auto& scenario:states){
                auto info=monitor;
                if(scenario.first==L"zero")info={0,0,0,0};
                if(scenario.first==L"100")info={1023,1023,100,100};
                if(scenario.first==L"K-M")info={1048576,1048064,100,100};
                if(scenario.first==L"M-G")info={1ULL<<30,(1ULL<<30)-524288,100,100};
                if(scenario.first==L"huge")info={std::numeric_limits<unsigned long long>::max(),std::numeric_limits<unsigned long long>::max(),100,100};
                state.monitor(info);state.battery(scenario.second);
                Surface image(width,height);auto oldfont=SelectObject(image.dc,font);image.fill(back);
                panel.DrawItem(image.dc,0,0,width,height,dark);
                auto name=theme+L"-"+std::to_wstring(dpi)+L"-"+scenario.first;
                image.save(out/(name+L".png"));
                Surface zoom(width*4,height*4);SetStretchBltMode(zoom.dc,COLORONCOLOR);StretchBlt(zoom.dc,0,0,width*4,height*4,image.dc,0,0,width,height,SRCCOPY);zoom.save(out/(name+L"-4x.png"));
                SelectObject(image.dc,oldfont);
            }
            Surface previous(before,height);auto oldfont=SelectObject(previous.dc,font);previous.fill(back);legacy_draw(previous.dc,dpi,dark,before,height);previous.save(out/(theme+L"-"+std::to_wstring(dpi)+L"-previous.png"));SelectObject(previous.dc,oldfont);
        }
        DeleteObject(font);
    }
    metrics << "\n  ]\n}\n";metrics.close();
    GdiplusShutdown(token);std::cout<<"PNG previews and metrics written; no HID query performed\n";
}
