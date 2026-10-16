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
    CHECK(carousel(snap,5000).device==BatteryDevice::mouse);
    CHECK(std::wstring(battery_label(BatteryDevice::mouse))==L"鼠标");
    CHECK(std::wstring(battery_label(BatteryDevice::controller))==L"手柄");
    CHECK(std::wstring(battery_label(BatteryDevice::earcase))==L"耳仓");
    snap.controller_registered=true;snap.controller={64,1000,0,1,false,false,0};
    CHECK(carousel(snap,0).device==BatteryDevice::mouse);
    CHECK(carousel(snap,4999).device==BatteryDevice::mouse);
    CHECK(carousel(snap,5000).device==BatteryDevice::controller);
    CHECK(carousel(snap,9999).sample==&snap.controller);
    CHECK(carousel(snap,10000).device==BatteryDevice::mouse);
    CHECK(tooltip(snap,1000).find(L"手柄电量：64%")!=std::wstring::npos);
    CHECK(tooltip(snap,41001).find(L"手柄电量：64%") == std::wstring::npos);
    // History is controller-only, and only an explicit clean offline result uses it.
    SharedState history;
    history.controller({vxe::offline,1000,0,0,false,false});
    auto h=history.read();
    CHECK(display_battery_value(h,carousel(h,5000),5000)==vxe::offline);
    history.controller({63,2000,0,1,false,false,1});
    history.controller({vxe::offline,3000,0,0,false,false});
    h=history.read();
    CHECK(controller_history_visible(h));
    CHECK(display_battery_value(h,carousel(h,5000),5000)==63);
    CHECK(display_battery_value(h,carousel(h,85000),85000)==63);
    CHECK(tooltip(h,85000).find(L"63%（历史最后一次检测")!=std::wstring::npos);
    CHECK(tooltip(h,85000).find(L"正在充电")==std::wstring::npos);
    CHECK(battery_value(h.controller,85000)==vxe::unavailable);
    CHECK(display_battery_value(h,carousel(h,80000),80000)==vxe::unavailable);
    for(const auto& invalid : {BatterySample{101,4000,0,1,false,false},
        BatterySample{vxe::unavailable,4000,5,1,false,false},
        BatterySample{vxe::offline,4000,1460,1,true,false},
        BatterySample{vxe::offline,4000,0,0,false,true}}) {
        history.controller(invalid); h=history.read();
        CHECK(!controller_history_visible(h)); CHECK(h.controller_last_valid.value==63);
    }
    history.controller({82,5000,0,1,false,false,0});h=history.read();
    CHECK(display_battery_value(h,carousel(h,5000),5000)==82);
    CHECK(!controller_history_visible(h));CHECK(h.controller_last_valid.value==82);
    CHECK(display_battery_value(h,carousel(h,85000),85000)==vxe::unavailable);
    history.controller({vxe::offline,6000,0,0,false,false}); h=history.read();
    CHECK(display_battery_value(h,carousel(h,85000),85000)==82);
    SharedState restored;restored.restore_controller(0);
    restored.controller({vxe::offline,1000,0,0,false,false});h=restored.read();
    CHECK(controller_history_visible(h));CHECK(display_battery_value(h,carousel(h,5000),5000)==0);
    restored.restore_controller(101); CHECK(restored.read().controller_last_valid.value==0);
    snap.earcase_registered=true;snap.earcase={94,1000,0,1,false,false};
    CHECK(carousel(snap,10000).device==BatteryDevice::earcase);
    CHECK(carousel(snap,14999).sample==&snap.earcase);
    CHECK(carousel(snap,15000).device==BatteryDevice::mouse);
    snap.controller_registered=false;
    CHECK(carousel(snap,5000).device==BatteryDevice::earcase);
    snap.controller_registered=false;snap.earcase_registered=false;
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
        CHECK(l.battery_width>=l.percent_width);
        CHECK(l.battery_width>=l.px(14));
        CHECK(l.px(14)<=l.height/2);
        // YaHei includes unused line leading beyond 16 DIP. Check rendered ink,
        // not tmHeight, before accepting the fixed 32-DIP host rectangle.
        {
            int row=MulDiv(32,dpi,96)/2,ref_width=l.speed_width+20,ref_height=l.text_height+40;
            BITMAPINFO ref_info{};ref_info.bmiHeader={sizeof(BITMAPINFOHEADER),ref_width,-ref_height,1,32,BI_RGB};
            void* ref_pixels=nullptr;auto ref_bitmap=CreateDIBSection(dc,&ref_info,DIB_RGB_COLORS,&ref_pixels,nullptr,0);
            auto previous_bitmap=SelectObject(dc,ref_bitmap);
            for(const auto& text : {L"0123456789",L"99.9M",L"1023B",L"17e9G",L"820K",L"100",L"?",L"--",L"鼠标",L"手柄",L"耳仓"}){
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
        state.monitor(snap.monitor); // Distinct upload/download values expose row mix-ups.
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
        // Pixel regression: top row is upload (arrow AND value), bottom is download.
        // Repaint only the network column with an independent explicit reference;
        // every pixel, including all other columns and padding, must stay identical.
        std::vector<DWORD> rendered(p,p+all.right*all.bottom);
        int half=MulDiv(32,dpi,96)/2;
        RECT network{10+l.pad,10,10+l.network_end(),10+2*half};
        auto network_background=CreateSolidBrush(RGB(20,30,40));
        FillRect(dc,&network,network_background);DeleteObject(network_background);
        int reference_state=SaveDC(dc);SetBkMode(dc,TRANSPARENT);
        for(int row=0;row<2;++row){
            RECT arrow{10+l.pad,10+row*half,10+l.pad+l.arrow,10+(row+1)*half};
            RECT value{10+l.pad+l.arrow+l.icon_gap,arrow.top,10+l.network_end(),arrow.bottom};
            draw_icon(dc,row==0?Icon::up:Icon::down,arrow,RGB(255,255,255),l);
            draw_text(dc,speed(row==0?snap.monitor.up_speed:snap.monitor.down_speed),value,RGB(255,255,255));
        }
        RestoreDC(dc,reference_state);GdiFlush();
        CHECK(std::equal(rendered.begin(),rendered.end(),p));
        // Exercise all selected production glyphs, their alpha/color blending,
        // both row bounds, and the same GDI-state contract at every tested DPI.
        snap.battery={85,30000,0,1,false,false};
        snap.controller_registered=true;snap.controller={64,30000,0,1,false,false,0};
        snap.earcase_registered=true;snap.earcase={94,30000,0,1,false,false};
        auto resources=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
        // Historical disconnected G5 must paint exactly the last valid number,
        // at the same width/DPI/color, without changing any other column.
        auto clear=CreateSolidBrush(RGB(20,30,40));FillRect(dc,&all,clear);DeleteObject(clear);
        draw(dc,10,10,width,MulDiv(32,dpi,96),true,RGB(234,201,167),l,snap,35000);
        GdiFlush();std::vector<DWORD> online_pixels(p,p+all.right*all.bottom);
        auto disconnected=snap;
        disconnected.controller_last_valid=snap.controller;
        disconnected.controller={vxe::offline,34000,0,0,false,false};
        clear=CreateSolidBrush(RGB(20,30,40));FillRect(dc,&all,clear);DeleteObject(clear);
        draw(dc,10,10,width,MulDiv(32,dpi,96),true,RGB(234,201,167),l,disconnected,35000);
        GdiFlush();CHECK(std::equal(online_pixels.begin(),online_pixels.end(),p));
        for(unsigned phase=0;phase<3;++phase){
            auto background=CreateSolidBrush(RGB(20,30,40));FillRect(dc,&all,background);DeleteObject(background);
            SetTextColor(dc,RGB(4,5,6));SetBkMode(dc,OPAQUE);
            const auto foreground=RGB(234,201,167);
            draw(dc,10,10,width,MulDiv(32,dpi,96),true,foreground,l,snap,30000+phase*5000);
            GdiFlush();
            CHECK(GetTextColor(dc)==RGB(4,5,6));CHECK(GetBkMode(dc)==OPAQUE);
            CHECK(GetCurrentObject(dc,OBJ_FONT)==font);
            int size=l.px(14),gx=10+l.battery_start()+(l.battery_width-size)/2;
            int gy=10+(MulDiv(32,dpi,96)/2-size)/2;
            const device_art::Mask* mask=nullptr;
            for(const auto& candidate:device_art::masks[phase])if(candidate.size==size)mask=&candidate;
            CHECK(mask!=nullptr);unsigned painted=0;
            for(int yy=0;yy<size;++yy)for(int xx=0;xx<size;++xx){
                unsigned a=mask->alpha[yy*size+xx];
                auto blend=[a](unsigned fg,unsigned bg){return (fg*a+bg*(255-a)+127)/255;};
                auto expected=(blend(234,20)<<16)|(blend(201,30)<<8)|blend(167,40);
                CHECK((p[(gy+yy)*all.right+gx+xx]&0xffffff)==expected);
                painted+=a!=0;
            }
            CHECK(painted>10);
            for(int yy=0;yy<all.bottom;++yy)for(int xx=0;xx<all.right;++xx)
                if(xx<10 || xx>=width+10 || yy<10 || yy>=all.bottom-10)
                    CHECK((p[yy*all.right+xx]&0xffffff)==0x141e28);
        }
        CHECK(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)==resources);
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
        CHECK(plugin->GetTooltipInfo()!=nullptr);
        CHECK(std::wstring(plugin->GetTooltipInfo()).empty());
        CHECK(std::wstring(plugin->GetItem(0)->GetItemValueText())==L"不可用");
        FreeLibrary(dll);
    }
    std::cout << checks << " checks passed; no HID query performed\n";
}
