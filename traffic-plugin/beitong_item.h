#pragma once
#include <windows.h>
#include "vendor/PluginInterface.h"
#include "beitong_state.h"
#include <array>
#include <string>

class BeitongItem : public IPluginItem {
    beitong::SharedState& state;
    std::array<std::wstring, 101> values;
public:
    explicit BeitongItem(beitong::SharedState& s) : state(s) {
        for (int i = 0; i <= 100; ++i) values[i] = std::to_wstring(i) + L"%";
    }
    const wchar_t* GetItemName() const override { return L"北通鲲鹏20电量"; }
    const wchar_t* GetItemId() const override { return L"beitong_kp20_battery"; }
    const wchar_t* GetItemLableText() const override { return L"手柄 "; }
    const wchar_t* GetItemValueSampleText() const override { return L"不可用"; }
    const wchar_t* GetItemValueText() const override {
        const auto value = beitong::visible_value(state.read(), GetTickCount64());
        return value >= 0 ? values[value].c_str() : value == beitong::offline ? L"离线" : L"不可用";
    }
};
