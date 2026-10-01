#pragma once
#include "compact_draw.h"
#include <array>
#include <atomic>
#include <cstring>

class MouseItem : public IPluginItem {
    compact::SharedState& state;
    std::array<std::wstring,101> texts;
public:
    explicit MouseItem(compact::SharedState& shared):state(shared) {
        for(int i=0;i<=100;++i)texts[i]=std::to_wstring(i)+L"%";
    }
    const wchar_t* GetItemName() const override{return L"VXE R1 电量";}
    const wchar_t* GetItemId() const override{return L"vxe_r1_battery";}
    const wchar_t* GetItemLableText() const override{return L"鼠标 ";}
    const wchar_t* GetItemValueText() const override{
        int value=compact::battery_value(state.read().battery,GetTickCount64());
        return value>=0 ? texts[value].c_str() : value==vxe::offline ? L"离线" : L"不可用";
    }
    const wchar_t* GetItemValueSampleText() const override{return L"不可用";}
};

class CompactItem : public IPluginItem {
    compact::SharedState& state;
    mutable std::mutex drawing_mutex;
    mutable LOGFONTW font_info{};
    mutable HFONT font=nullptr;
    mutable compact::Layout layout;
    std::atomic<COLORREF> text_color{CLR_INVALID};
public:
    explicit CompactItem(compact::SharedState& shared):state(shared){}
    ~CompactItem(){if(font)DeleteObject(font);}
    const wchar_t* GetItemName() const override{return L"极简数字面板（网速/CPU/内存/电量）";}
    const wchar_t* GetItemId() const override{return L"vxe_compact_panel";}
    const wchar_t* GetItemLableText() const override{return L"";}
    const wchar_t* GetItemValueText() const override{return L"";}
    const wchar_t* GetItemValueSampleText() const override{return L"";}
    bool IsCustomDraw() const override{return true;}
    int GetItemWidth() const override{return 130;}
    int GetItemWidthEx(void* handle) const override{
        if(!handle)return 0;
        HDC dc=static_cast<HDC>(handle);
        std::lock_guard<std::mutex> lock(drawing_mutex);
        LOGFONTW selected{};
        if(!GetObjectW(GetCurrentObject(dc,OBJ_FONT),sizeof(selected),&selected))return 0;
        if(!font || memcmp(&selected,&font_info,sizeof(selected))!=0){
            auto replacement=CreateFontIndirectW(&selected);
            if(!replacement)return 0;
            auto measured=compact::measure(dc);
            if(font)DeleteObject(font);
            font=replacement; font_info=selected; layout=measured;
        }
        return layout.width;
    }
    void color(COLORREF value){text_color.store(value);}
    void DrawItem(void* handle,int x,int y,int w,int h,bool dark) override{
        if(!handle || w<=0 || h<=0)return;
        auto snapshot=state.read();
        std::lock_guard<std::mutex> lock(drawing_mutex);
        HDC dc=static_cast<HDC>(handle);
        int saved=SaveDC(dc);
        if(font)SelectObject(dc,font);
        auto measured=font?layout:compact::measure(dc);
        auto color=text_color.load();
        if(color==CLR_INVALID)color=GetTextColor(dc);
        compact::draw(dc,x,y,w,h,dark,color,measured,snapshot,GetTickCount64());
        RestoreDC(dc,saved);
    }
    int OnMouseEvent(MouseEventType,int,int,void*,int) override{return 0;}
};
