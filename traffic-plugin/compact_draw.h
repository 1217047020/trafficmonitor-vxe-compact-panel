#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "compact_model.h"
#include "device_icon_draw.h"
#include <cstdlib>

namespace compact {
struct Layout {
    int width = 0, height = 0, speed_width = 0, percent_width = 0, battery_width = 0;
    int arrow = 0, icon = 0, gap = 0, icon_gap = 0, pad = 0, text_height = 0;
    double scale = 1;
    int px(int logical) const { return std::max(1, static_cast<int>(std::lround(logical * scale))); }
    int network_end() const { return pad + arrow + icon_gap + speed_width; }
    int usage_start() const { return network_end() + gap; }
    int usage_end() const { return usage_start() + icon + icon_gap + percent_width; }
    int battery_start() const { return usage_end() + gap; }
};
inline int text_width(HDC dc, const std::wstring& text) {
    SIZE extent{}; GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &extent); return extent.cx;
}
inline Layout measure(HDC dc) {
    Layout l;
    LOGFONTW font{}; GetObjectW(GetCurrentObject(dc, OBJ_FONT), sizeof(font), &font);
    // Font comes from host GetItemWidthEx, including its actual per-monitor DPI.
    l.scale = std::max<LONG>(1, std::abs(font.lfHeight)) / 12.0;
    TEXTMETRICW tm{}; GetTextMetricsW(dc, &tm); l.text_height = tm.tmHeight;
    l.arrow = l.px(7); l.icon = l.px(11); l.gap = l.px(6); l.icon_gap = l.px(2); l.pad = l.px(1);
    for (const auto& s : speed_bounds()) l.speed_width = std::max(l.speed_width, text_width(dc, s));
    l.percent_width = std::max(text_width(dc, L"--"), text_width(dc, L"?"));
    for (int n = 0; n <= 100; ++n) l.percent_width = std::max(l.percent_width, text_width(dc, std::to_wstring(n)));
    l.battery_width = std::max(l.percent_width, l.px(14));
    l.width = l.battery_start() + l.battery_width + l.pad;
    l.height = l.px(32); // Numerical ink fits; font leading is checked by GDI tests.
    return l;
}
inline COLORREF battery_color(int value, COLORREF normal, bool dark) {
    if (value >= 0 && value <= 10) return dark ? RGB(255,100,100) : RGB(196,32,32);
    if (value >= 0 && value <= 20) return dark ? RGB(255,180,80) : RGB(180,85,0);
    return normal;
}
inline void line(HDC dc, int x1, int y1, int x2, int y2) { MoveToEx(dc, x1, y1, nullptr); LineTo(dc, x2, y2); }
enum class Icon { down, up, cpu, memory };
inline RECT cpu_glyph_rect(const RECT& cell) {
    int size=std::min(static_cast<int>(cell.right-cell.left),static_cast<int>(cell.bottom-cell.top));
    int left=cell.left+(cell.right-cell.left-size)/2;
    int top=cell.top+(cell.bottom-cell.top-size)/2;
    return RECT{left,top,left+size,top+size};
}
inline void draw_cpu_icon(HDC dc,const RECT& cell,COLORREF color) {
    // An explicitly mirror-symmetric 11x11 mask: package + central die,
    // three pins on each of four sides. No GDI pen/end-cap bias.
    static constexpr const char* mask[]={
        "...#.#.#...",
        "...#.#.#...",
        "..#######..",
        "###.....###",
        "..#.###.#..",
        "###.#.#.###",
        "..#.###.#..",
        "###.....###",
        "..#######..",
        "...#.#.#...",
        "...#.#.#..."
    };
    RECT box=cpu_glyph_rect(cell);int size=box.right-box.left;
    if(size<=0)return;
    auto brush=CreateSolidBrush(color);
    auto source=[size](int position){return ((2*position+1)*11)/(2*size);};
    // Pixel-centre nearest sampling of an odd symmetric mask preserves both
    // reflections exactly, including non-integer 125/150% raster sizes.
    for(int y=0;y<size;++y){
        int row=source(y),x=0;
        while(x<size){
            if(mask[row][source(x)]!='#'){++x;continue;}
            int begin=x++;
            while(x<size && mask[row][source(x)]=='#')++x;
            RECT run{box.left+begin,box.top+y,box.left+x,box.top+y+1};
            FillRect(dc,&run,brush);
        }
    }
    DeleteObject(brush);
}
inline void draw_icon(HDC dc, Icon icon, const RECT& rect, COLORREF color, const Layout& l) {
    if(icon==Icon::cpu){draw_cpu_icon(dc,rect,color);return;}
    auto pen = CreatePen(PS_SOLID, l.px(1), color);
    auto old_pen = SelectObject(dc, pen); auto old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    int cx = (rect.left + rect.right) / 2, cy = (rect.top + rect.bottom) / 2;
    int r = l.px(4);
    if (icon == Icon::down || icon == Icon::up) {
        int sign = icon == Icon::down ? 1 : -1;
        line(dc, cx, cy - sign*r, cx, cy + sign*r);
        line(dc, cx-l.px(3), cy+sign*l.px(1), cx, cy+sign*r);
        line(dc, cx, cy+sign*r, cx+l.px(3), cy+sign*l.px(1));
    } else if (icon == Icon::memory) {
        Rectangle(dc,cx-r,cy-l.px(3),cx+r+1,cy+l.px(2));
        for (int n : {-2,0,2}) {
            line(dc,cx+l.px(1)*n,cy-l.px(1),cx+l.px(1)*n,cy+l.px(1));
            line(dc,cx+l.px(1)*n,cy+l.px(2),cx+l.px(1)*n,cy+l.px(4));
        }
    }
    SelectObject(dc, old_brush); SelectObject(dc, old_pen); DeleteObject(pen);
}
inline void draw_text(HDC dc, const std::wstring& text, RECT rect, COLORREF color) {
    SetTextColor(dc, color);
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &rect, DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
}
inline void draw(HDC dc, int x, int y, int w, int h, bool dark, COLORREF normal,
                 const Layout& l, const Snapshot& s, std::uint64_t now) {
    int saved = SaveDC(dc);
    IntersectClipRect(dc,x,y,x+w,y+h); SetBkMode(dc,TRANSPARENT);
    // Extra host width is left padding, never redistribution of column widths.
    int left = x + std::max(0,w-l.width), half = h/2;
    RECT top{left,y,left+l.width,y+half}, bottom{left,y+half,left+l.width,y+h};
    auto cell = [&](const RECT& row, int begin, int end) { return RECT{left+begin,row.top,left+end,row.bottom}; };
    draw_icon(dc,Icon::down,cell(top,l.pad,l.pad+l.arrow),normal,l);
    draw_icon(dc,Icon::up,cell(bottom,l.pad,l.pad+l.arrow),normal,l);
    int speed_start = l.pad+l.arrow+l.icon_gap;
    draw_text(dc,s.monitor_received?speed(s.monitor.down_speed):L"?",cell(top,speed_start,l.network_end()),normal);
    draw_text(dc,s.monitor_received?speed(s.monitor.up_speed):L"?",cell(bottom,speed_start,l.network_end()),normal);
    draw_icon(dc,Icon::cpu,cell(top,l.usage_start(),l.usage_start()+l.icon),normal,l);
    draw_icon(dc,Icon::memory,cell(bottom,l.usage_start(),l.usage_start()+l.icon),normal,l);
    int usage_value = l.usage_start()+l.icon+l.icon_gap;
    draw_text(dc,s.monitor_received?percentage(s.monitor.cpu_usage):L"?",cell(top,usage_value,l.usage_end()),normal);
    draw_text(dc,s.monitor_received?percentage(s.monitor.memory_usage):L"?",cell(bottom,usage_value,l.usage_end()),normal);
    const auto current = carousel(s, now);
    draw_device_icon(dc,current.device,cell(top,l.battery_start(),l.battery_start()+l.battery_width),l.px(14),normal);
    auto number=cell(bottom,l.battery_start(),l.battery_start()+l.battery_width);
    auto displayed=display_battery_value(s,current,now);
    auto value=displayed==vxe::offline ? L"--" : percentage(displayed);
    SetTextColor(dc,battery_color(displayed,normal,dark));
    DrawTextW(dc,value.c_str(),static_cast<int>(value.size()),&number,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
    RestoreDC(dc,saved);
}
}
