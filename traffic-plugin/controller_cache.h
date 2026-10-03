#pragma once
#include <windows.h>
#include <cstdint>
#include <string>

namespace controller_cache {
// Only the validated percentage is persisted. Never restore charging/online state.
struct Record {
    std::uint32_t magic = 0x3142504b, version = 1, percentage = 0, reserved = 0;
    std::uint64_t observed = 0;
};
static_assert(sizeof(Record) == 24, "Unexpected cache layout");
inline std::uint64_t wall_time() {
    FILETIME time{}; GetSystemTimeAsFileTime(&time);
    return (static_cast<std::uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
}
inline std::wstring path() {
    wchar_t directory[32768]{};
    const auto count = GetEnvironmentVariableW(L"LOCALAPPDATA", directory, 32768);
    if (!count || count >= 32768) return {};
    std::wstring base = std::wstring(directory) + L"\\TrafficMonitor";
    if (!CreateDirectoryW(base.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return {};
    base += L"\\VxeBattery";
    if (!CreateDirectoryW(base.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return {};
    return base + L"\\controller-last-valid.bin";
}
inline int load(const std::wstring& file) {
    if (file.empty()) return -1;
    auto handle = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return -1;
    Record record{}; DWORD read = 0; LARGE_INTEGER size{};
    const bool ok = GetFileSizeEx(handle, &size) && size.QuadPart == sizeof(record) &&
        ReadFile(handle, &record, sizeof(record), &read, nullptr) && read == sizeof(record);
    CloseHandle(handle);
    if (!ok || record.magic != 0x3142504b || record.version != 1 || record.reserved ||
        record.percentage > 100 || !record.observed || record.observed > wall_time()) return -1;
    return static_cast<int>(record.percentage);
}
inline bool save(const std::wstring& file, int percentage) {
    if (file.empty() || percentage < 0 || percentage > 100) return false;
    Record record{}; record.percentage = percentage; record.observed = wall_time();
    const auto temporary = file + L".tmp";
    auto handle = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const bool ok = WriteFile(handle, &record, sizeof(record), &written, nullptr) &&
        written == sizeof(record) && FlushFileBuffers(handle);
    CloseHandle(handle);
    if (ok && MoveFileExW(temporary.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
    DeleteFileW(temporary.c_str()); return false;
}
}
