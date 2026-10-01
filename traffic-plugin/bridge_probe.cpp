#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "vendor/PluginInterface.h"
#include <chrono>
#include <thread>
#include <iostream>
#include <string>
struct TestRegistry {
    HKEY root=nullptr, isolated=nullptr;
    std::wstring path;
    bool active=false;
    ~TestRegistry() {
        if (active) RegOverridePredefKey(HKEY_CURRENT_USER,nullptr);
        if (isolated) RegCloseKey(isolated);
        if (root) {
            if (active) RegDeleteTreeW(root,path.c_str());
            RegCloseKey(root);
        }
    }
    bool set_port(DWORD port) {
        if (port<1024 || port>65535) return false;
        path=L"Software\\VxeBatteryBridgeTests\\"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
        DWORD disposition=0;
        if (RegOpenKeyExW(HKEY_CURRENT_USER,L"",0,KEY_ALL_ACCESS,&root) ||
            RegCreateKeyExW(root,path.c_str(),0,nullptr,REG_OPTION_VOLATILE,KEY_ALL_ACCESS,nullptr,&isolated,&disposition) ||
            disposition!=REG_CREATED_NEW_KEY || RegOverridePredefKey(HKEY_CURRENT_USER,isolated)) return false;
        active=true;
        HKEY key=nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\BatteryMonitor\\BatteryMonitor",0,nullptr,
                           REG_OPTION_VOLATILE,KEY_ALL_ACCESS,nullptr,&key,nullptr)) return false;
        auto result=RegSetValueExW(key,L"RpcPort",0,REG_DWORD,reinterpret_cast<BYTE*>(&port),sizeof(port));
        RegCloseKey(key);
        return result==ERROR_SUCCESS;
    }
};
int wmain(int argc, wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);
    if (argc < 2) return 2;
    TestRegistry isolated;
    if (argc > 3 && !isolated.set_port(_wtoi(argv[3]))) return 7;
    auto dll = LoadLibraryW(argv[1]);
    if (!dll) { std::cerr << "LoadLibrary failed: " << GetLastError(); return 3; }
    auto factory = reinterpret_cast<ITMPlugin*(*)()>(GetProcAddress(dll,"TMPluginGetInstance"));
    if (!factory) return 4;
    auto plugin = factory();
    if (!plugin || plugin->GetAPIVersion() != 7 || !plugin->GetItem(0) || plugin->GetItem(2)) return 5;
    plugin->DataRequired();
    int seconds = argc > 2 ? _wtoi(argv[2]) : 12;
    std::wstring previous;
    for (int i=0; i < seconds*4; ++i) {
        std::wstring value = plugin->GetItem(0)->GetItemValueText();
        if (value != previous) {
            char bytes[128];
            int count = WideCharToMultiByte(CP_UTF8,0,value.c_str(),-1,bytes,sizeof(bytes),nullptr,nullptr);
            if (!count) return 6;
            std::cout << bytes << std::endl;
            previous=value;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
    // Pinned production module intentionally survives FreeLibrary until process exit.
    FreeLibrary(dll);
}
