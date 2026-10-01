#include "plugin_items.h"
#include <iostream>
#include <limits>
#include <random>
#include <set>
#include <thread>

static unsigned checks=0;
#define CHECK(test) do { ++checks; if(!(test)){std::cerr << "FAIL line " << __LINE__ << ": " << #test << '\n';return 1;} } while(0)
int wmain(int argc,wchar_t** argv){
    using namespace compact;
    CHECK(speed(0)==L"0B"); CHECK(speed(1023)==L"1023B"); CHECK(speed(1024)==L"1K");
    CHECK(speed(1024*1024)==L"1M"); CHECK(speed(1ULL<<30)==L"1G");
    CHECK(speed(820*1024)==L"820K"); CHECK(speed(123*1024*1024/10)==L"12.3M");
    CHECK(speed(1024*1024-1)==L"1M"); CHECK(speed((1ULL<<30)-1)==L"1G");
    CHECK(speed(std::numeric_limits<std::uint64_t>::max())==L"17e9G");
    std::set<std::wstring> bounds(speed_bounds().begin(),speed_bounds().end());
    std::mt19937_64 random(20261001);
    for(unsigned n=0;n<250000;++n){
        auto bytes=n<10000 ? n : random(); auto text=speed(bytes);
        CHECK(text.size()<=5); CHECK(bounds.count(text)==1);
    }
    for(unsigned unit=1;unit<=3;++unit){
        auto factor=1ULL<<(10*unit);
        for(unsigned n=1;n<10240;++n){
            auto bytes=(n*factor)/10;
            for(int delta : {-1,0,1}){auto text=speed(bytes+delta);CHECK(bounds.count(text)==1);}
        }
    }
    BatterySample sample{85,1000,0,1,false,false};
    CHECK(battery_text(sample,1000)==L"85"); CHECK(battery_text(sample,41000)==L"85");
    CHECK(battery_text(sample,41001)==L"?"); CHECK(battery_text(sample,999)==L"?");
    sample.value=vxe::offline; CHECK(battery_text(sample,1000)==L"--");
    sample.value=vxe::unavailable; CHECK(battery_text(sample,1000)==L"?");
    sample.value=101; CHECK(battery_text(sample,1000)==L"?");
    CHECK(battery_text(BatterySample{},1000)==L"?");
    CHECK(percentage(-1)==L"?"); CHECK(percentage(101)==L"?"); CHECK(percentage(0)==L"0"); CHECK(percentage(100)==L"100");
    CHECK(battery_color(21,RGB(1,2,3),false)==RGB(1,2,3));
    CHECK(battery_color(20,0,false)==RGB(180,85,0)); CHECK(battery_color(10,0,false)==RGB(196,32,32));
    CHECK(battery_color(0,0,true)==RGB(255,100,100)); CHECK(battery_color(-1,123,true)==123);
    Snapshot snap; snap.monitor_received=true;snap.monitor={820*1024,123*1024*1024/10,100,62};
    snap.battery={85,1000,0,1,false,false};
    auto tip=tooltip(snap,1000);
    CHECK(tip.find(L"CPU占用：100%")!=std::wstring::npos);
    CHECK(tip.find(L"鼠标电量：85%")!=std::wstring::npos);
    CHECK(tip.find(L"839680 B/s")!=std::wstring::npos);
    CHECK(tooltip(snap,41001).find(L"鼠标电量：不可用（?）")!=std::wstring::npos);
    CHECK(tooltip(snap,41001).find(L"鼠标电量：85%") == std::wstring::npos);
    SharedState state; CompactItem panel(state); MouseItem legacy(state);
    CHECK(std::wstring(legacy.GetItemId())==L"vxe_r1_battery");
    CHECK(std::wstring(panel.GetItemId())==L"vxe_compact_panel"); CHECK(panel.IsCustomDraw());
    for(auto event:{IPluginItem::MT_LCLICKED,IPluginItem::MT_RCLICKED,IPluginItem::MT_DBCLICKED,IPluginItem::MT_WHEEL_UP,IPluginItem::MT_WHEEL_DOWN})
        CHECK(panel.OnMouseEvent(event,0,0,nullptr,IPluginItem::MF_TASKBAR_WND)==0);
    HDC dc=CreateCompatibleDC(nullptr);
    for(int dpi : {96,120,144,192}){
        auto font=CreateFontW(-MulDiv(9,dpi,72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH|FF_SWISS,L"微软雅黑");
        auto old=SelectObject(dc,font);auto l=measure(dc);auto width=panel.GetItemWidthEx(dc);
        CHECK(width==l.width);
        // YaHei includes unused line leading beyond 16 DIP. Check rendered ink,
        // not tmHeight, before accepting the fixed 32-DIP host rectangle.
        {
            int row=MulDiv(32,dpi,96)/2,ref_width=l.speed_width+20,ref_height=l.text_height+40;
            BITMAPINFO ref_info{};ref_info.bmiHeader={sizeof(BITMAPINFOHEADER),ref_width,-ref_height,1,32,BI_RGB};
            void* ref_pixels=nullptr;auto ref_bitmap=CreateDIBSection(dc,&ref_info,DIB_RGB_COLORS,&ref_pixels,nullptr,0);
            auto previous_bitmap=SelectObject(dc,ref_bitmap);
            for(const auto& text : {L"0123456789",L"99.9M",L"1023B",L"17e9G",L"820K",L"100",L"?",L"--"}){
                // Reference target is not clipped at row boundaries. Any painted
                // pixel above/below the row would be lost by real panel clipping.
                memset(ref_pixels,0,ref_width*ref_height*4);
                SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));
                RECT cell{0,20,ref_width,20+row};
                DrawTextW(dc,text,-1,&cell,DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX|DT_NOCLIP);
                GdiFlush();auto ref=static_cast<DWORD*>(ref_pixels);
                for(int y=0;y<ref_height;++y)for(int x=0;x<ref_width;++x)
                    if((ref[y*ref_width+x]&0xffffff)!=0)CHECK(y>=20 && y<20+row);
            }
            SelectObject(dc,previous_bitmap);DeleteObject(ref_bitmap);
        }
        for(const auto& text:speed_bounds())CHECK(text_width(dc,text)<=l.speed_width);
        for(int n=0;n<=100;++n)CHECK(text_width(dc,std::to_wstring(n))<=l.percent_width);
        for(auto bytes:{0ULL,1023ULL,1024ULL,1048575ULL,1048576ULL,1ULL<<30,std::numeric_limits<unsigned long long>::max()}){
            ITMPlugin::MonitorInfo info{bytes,bytes,100,100};state.monitor(info);
            CHECK(panel.GetItemWidthEx(dc)==width);
        }
        // Draw only inside supplied host rect and restore all GDI state.
        BITMAPINFO bi{};bi.bmiHeader={sizeof(BITMAPINFOHEADER),width+20,-(MulDiv(32,dpi,96)+20),1,32,BI_RGB};
        void* pixels=nullptr;auto bitmap=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&pixels,nullptr,0);
        auto old_bitmap=SelectObject(dc,bitmap);
        auto brush=CreateSolidBrush(RGB(20,30,40));RECT all{0,0,width+20,MulDiv(32,dpi,96)+20};FillRect(dc,&all,brush);DeleteObject(brush);
        SetTextColor(dc,RGB(4,5,6));SetBkMode(dc,OPAQUE);
        state.battery({100,GetTickCount64(),0,1,false,false});panel.color(RGB(255,255,255));
        panel.DrawItem(dc,10,10,width,MulDiv(32,dpi,96),true);GdiFlush();
        CHECK(GetTextColor(dc)==RGB(4,5,6));CHECK(GetBkMode(dc)==OPAQUE);CHECK(GetCurrentObject(dc,OBJ_FONT)==font);
        auto p=static_cast<DWORD*>(pixels);unsigned changed=0;
        for(int y=0;y<all.bottom;++y)for(int x=0;x<all.right;++x){
            bool inside=x>=10&&x<width+10&&y>=10&&y<all.bottom-10;
            auto pixel=p[y*all.right+x]&0xffffff;
            if(!inside)CHECK(pixel==0x141e28);else if(pixel!=0x141e28)++changed;
        }
        CHECK(changed>100);
        // Verify the actual production pixels, not only mathematical endpoints.
        RECT cpu_cell{10+l.usage_start(),10,10+l.usage_start()+l.icon,10+MulDiv(32,dpi,96)/2};
        RECT cpu_box=cpu_glyph_rect(cpu_cell);
        for(int y=cpu_box.top;y<cpu_box.bottom;++y)for(int x=cpu_box.left;x<cpu_box.right;++x){
            auto actual=p[y*all.right+x]&0xffffff;
            CHECK(actual==(p[y*all.right+cpu_box.right-1-(x-cpu_box.left)]&0xffffff));
            CHECK(actual==(p[(cpu_box.bottom-1-(y-cpu_box.top))*all.right+x]&0xffffff));
            CHECK(actual==(p[(cpu_box.top+x-cpu_box.left)*all.right+cpu_box.left+y-cpu_box.top]&0xffffff));
        }
        int cpu_x=cpu_box.left+(cpu_box.right-cpu_box.left)/2, cpu_y=cpu_box.top+(cpu_box.bottom-cpu_box.top)/2;
        CHECK((p[cpu_y*all.right+cpu_x]&0xffffff)==0x141e28);
        SelectObject(dc,old_bitmap);DeleteObject(bitmap);SelectObject(dc,old);DeleteObject(font);
    }
    DeleteDC(dc);
    // ABI smoke test loads the production DLL WITHOUT calling DataRequired:
    // no test/device-query process competes with the running installed worker.
    if(argc>1){
        auto dll=LoadLibraryW(argv[1]);CHECK(dll!=nullptr);
        auto factory=reinterpret_cast<ITMPlugin*(*)()>(GetProcAddress(dll,"TMPluginGetInstance"));CHECK(factory!=nullptr);
        auto plugin=factory();CHECK(plugin->GetAPIVersion()==7);
        CHECK(plugin->GetItem(-1)==nullptr);CHECK(plugin->GetItem(2)==nullptr);
        CHECK(std::wstring(plugin->GetItem(0)->GetItemId())==L"vxe_r1_battery");
        CHECK(std::wstring(plugin->GetItem(1)->GetItemId())==L"vxe_compact_panel");
        plugin->OnMonitorInfo(snap.monitor);
        CHECK(std::wstring(plugin->GetTooltipInfo()).find(L"CPU占用：100%")!=std::wstring::npos);
        CHECK(std::wstring(plugin->GetItem(0)->GetItemValueText())==L"不可用");
        FreeLibrary(dll);
    }
    std::cout << checks << " checks passed; no HID query performed\n";
}
