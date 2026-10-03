#include "../traffic-plugin/controller_cache.h"
#include <iostream>
#include <stdexcept>
void check(bool ok) { if(!ok) throw std::runtime_error("Controller cache assertion failed"); }
int wmain() {
    wchar_t directory[MAX_PATH]{}, file[MAX_PATH]{};
    check(GetTempPathW(MAX_PATH,directory)>0);
    check(GetTempFileNameW(directory,L"kp2",0,file)!=0);
    // Fresh unique test file only; never touches the user's installed cache.
    struct Cleanup { std::wstring file; ~Cleanup(){DeleteFileW(file.c_str());DeleteFileW((file+L".tmp").c_str());} } cleanup{file};
    using namespace controller_cache;
    check(load(file)==-1);check(load(L"")==-1);check(!save(L"",63));
    for(int percentage : {0,63,100,82}){check(save(file,percentage));check(load(file)==percentage);}
    check(!save(file,-1));check(!save(file,101));check(load(file)==82);
    auto corrupt=[&](const Record& record,DWORD count){
        auto handle=CreateFileW(file,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,0,nullptr);
        check(handle!=INVALID_HANDLE_VALUE);DWORD written=0;
        check(WriteFile(handle,&record,count,&written,nullptr));CloseHandle(handle);
        check(written==count);check(load(file)==-1);
    };
    Record record{};record.percentage=63;record.observed=wall_time();
    corrupt(record,1);record.magic=0;corrupt(record,sizeof(record));
    record=Record{};record.observed=wall_time();record.version=2;corrupt(record,sizeof(record));
    record.version=1;record.percentage=101;corrupt(record,sizeof(record));
    record.percentage=63;record.observed=0;corrupt(record,sizeof(record));
    record.observed=wall_time()+600000000ULL;corrupt(record,sizeof(record));
    record.observed=wall_time();record.reserved=1;corrupt(record,sizeof(record));
    check(save(file,74));check(load(file)==74);
    std::cout << "Controller persistent cache tests passed (isolated temp file; no device access)\n";
}
