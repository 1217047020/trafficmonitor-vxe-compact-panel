#pragma once
#include "device_icon_masks.h"
#include "compact_model.h"

namespace compact {
inline void draw_device_icon(HDC dc, BatteryDevice device, const RECT& cell, int requested, COLORREF color) {
    int size = std::min({requested, int(cell.right-cell.left), int(cell.bottom-cell.top)});
    if(size <= 0 || size > 512) return;
    unsigned index = device == BatteryDevice::mouse ? 0 : device == BatteryDevice::controller ? 1 : 2;
    const auto* mask = &device_art::masks[index][7];
    for(const auto& candidate : device_art::masks[index])
        if(candidate.size >= size) { mask = &candidate; break; }
    int left = cell.left + (cell.right-cell.left-size)/2;
    int top = cell.top + (cell.bottom-cell.top-size)/2;
    HDC buffer = CreateCompatibleDC(dc);
    if(!buffer) return;
    BITMAPINFO info{};
    info.bmiHeader={sizeof(BITMAPINFOHEADER),size,-size,1,32,BI_RGB};
    void* pixels=nullptr;
    HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    if(!bitmap) { DeleteDC(buffer); return; }
    auto previous=SelectObject(buffer,bitmap);
    // Blend over the actual host background, retaining transparency and color.
    if(BitBlt(buffer,0,0,size,size,dc,left,top,SRCCOPY)) {
        GdiFlush();
        auto target=static_cast<std::uint32_t*>(pixels);
        for(int y=0;y<size;++y) for(int x=0;x<size;++x) {
            // Known DPI sizes are exact; other sizes use the next finer mask.
            double sx=(x+.5)*mask->size/size-.5, sy=(y+.5)*mask->size/size-.5;
            int x0=std::clamp(int(std::floor(sx)),0,mask->size-1);
            int y0=std::clamp(int(std::floor(sy)),0,mask->size-1);
            int x1=std::min(x0+1,mask->size-1), y1=std::min(y0+1,mask->size-1);
            double fx=std::clamp(sx-x0,0.0,1.0), fy=std::clamp(sy-y0,0.0,1.0);
            auto a=[&](int xx,int yy){return mask->alpha[yy*mask->size+xx];};
            unsigned alpha=static_cast<unsigned>(std::lround((1-fy)*((1-fx)*a(x0,y0)+fx*a(x1,y0)) + fy*((1-fx)*a(x0,y1)+fx*a(x1,y1))));
            auto& pixel=target[y*size+x];
            auto blend=[&](unsigned shift,unsigned foreground){return (foreground*alpha+((pixel>>shift)&255)*(255-alpha)+127)/255;};
            unsigned red=blend(16,GetRValue(color)), green=blend(8,GetGValue(color)), blue=blend(0,GetBValue(color));
            pixel=(red<<16)|(green<<8)|blue;
        }
        BitBlt(dc,left,top,size,size,buffer,0,0,SRCCOPY);
    }
    SelectObject(buffer,previous);DeleteObject(bitmap);DeleteDC(buffer);
}
}
