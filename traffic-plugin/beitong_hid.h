#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <setupapi.h>
extern "C" {
#include <hidsdi.h>
#include <hidpi.h>
}
#include "beitong_protocol.h"
#include <memory>
#include <vector>

// One-shot native transport, not called by the installed plugin.
namespace beitong {
struct Handle {
    HANDLE value;
    explicit Handle(HANDLE h) : value(h) {}
    bool valid() const { return value && value != INVALID_HANDLE_VALUE; }
    ~Handle() { if (valid()) CloseHandle(value); }
    Handle(const Handle&) = delete;
};
struct QueryResult {
    Status status;
    unsigned matches = 0, reports = 0;
    DWORD error = 0, written = 0, read = 0;
    bool timed_out = false, stop = false;
    Packet reply{};
};
struct Io {
    OVERLAPPED ov{};
    Packet buffer{};
    Io() { ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr); }
    ~Io() { if (ov.hEvent) CloseHandle(ov.hEvent); }
};
inline void cancel_io(HANDLE device, std::unique_ptr<Io>& io, QueryResult& result) {
    CancelIoEx(device, &io->ov);
    if (WaitForSingleObject(io->ov.hEvent, 100) != WAIT_OBJECT_0) {
        // Keep kernel-owned OVERLAPPED/event/buffer alive. A worker must stop,
        // never retry indefinitely if cancellation has not completed.
        result.stop = true;
        io.release();
    }
}
inline bool complete_io(HANDLE device, std::unique_ptr<Io>& io, BOOL immediate,
                        DWORD& count, DWORD timeout, QueryResult& result) {
    if (immediate) return true;
    result.error = GetLastError();
    if (result.error != ERROR_IO_PENDING) return false;
    const auto wait = WaitForSingleObject(io->ov.hEvent, timeout);
    if (wait != WAIT_OBJECT_0) {
        result.timed_out = wait == WAIT_TIMEOUT;
        result.error = result.timed_out ? ERROR_TIMEOUT : GetLastError();
        cancel_io(device, io, result);
        return false;
    }
    if (!GetOverlappedResult(device, &io->ov, &count, FALSE)) {
        result.error = GetLastError();
        return false;
    }
    result.error = 0;
    return true;
}
inline QueryResult query_status() noexcept {
    QueryResult result;
    try {
        Handle mutex(CreateMutexW(nullptr, FALSE, L"Local\\BEITONG_KP20D_STATUS_20BC_5159"));
        if (!mutex.valid()) { result.error = GetLastError(); return result; }
        const auto owned = WaitForSingleObject(mutex.value, 0);
        if (owned != WAIT_OBJECT_0 && owned != WAIT_ABANDONED) {
            result.error = ERROR_BUSY; return result;
        }
        struct Unlock { HANDLE h; ~Unlock() { ReleaseMutex(h); } } unlock{mutex.value};
        GUID guid{}; HidD_GetHidGuid(&guid);
        const auto set = SetupDiGetClassDevsW(&guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
        if (set == INVALID_HANDLE_VALUE) { result.error = GetLastError(); return result; }
        struct CloseSet { HDEVINFO h; ~CloseSet() { SetupDiDestroyDeviceInfoList(h); } } closer{set};
        std::wstring selected;
        for (DWORD index = 0; ; ++index) {
            SP_DEVICE_INTERFACE_DATA entry{}; entry.cbSize = sizeof(entry);
            if (!SetupDiEnumDeviceInterfaces(set, nullptr, &guid, index, &entry)) {
                if (GetLastError() != ERROR_NO_MORE_ITEMS) result.error = GetLastError();
                break;
            }
            DWORD size = 0;
            SetupDiGetDeviceInterfaceDetailW(set, &entry, nullptr, 0, &size, nullptr);
            if (size < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) || size > 65536) continue;
            std::vector<BYTE> memory(size);
            auto detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(memory.data());
            detail->cbSize = sizeof(*detail);
            if (!SetupDiGetDeviceInterfaceDetailW(set, &entry, detail, size, nullptr, nullptr)) continue;
            if (!target_path(detail->DevicePath)) continue;
            Handle metadata(CreateFileW(detail->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                         nullptr, OPEN_EXISTING, 0, nullptr));
            if (!metadata.valid()) { result.error = GetLastError(); return result; }
            HIDD_ATTRIBUTES attrs{}; attrs.Size = sizeof(attrs);
            if (!HidD_GetAttributes(metadata.value, &attrs)) { result.error = GetLastError(); return result; }
            PHIDP_PREPARSED_DATA pp = nullptr;
            if (!HidD_GetPreparsedData(metadata.value, &pp)) { result.error = GetLastError(); return result; }
            HIDP_CAPS caps{};
            const auto parsed = HidP_GetCaps(pp, &caps);
            HidD_FreePreparsedData(pp);
            if (parsed != HIDP_STATUS_SUCCESS || !target_caps(attrs.VendorID, attrs.ProductID,
                caps.UsagePage, caps.Usage, caps.InputReportByteLength,
                caps.OutputReportByteLength, caps.FeatureReportByteLength)) continue;
            ++result.matches; selected = detail->DevicePath;
        }
        if (result.error || !result.matches) return result;
        if (result.matches != 1) { result.error = ERROR_DUPLICATE_TAG; return result; }
        Handle device(CreateFileW(selected.c_str(), GENERIC_READ | GENERIC_WRITE,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                  OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr));
        if (!device.valid()) { result.error = GetLastError(); return result; }
        if (!HidD_SetNumInputBuffers(device.value, 64) || !HidD_FlushQueue(device.value)) {
            result.error = GetLastError(); return result;
        }
        auto reader = std::make_unique<Io>();
        auto writer = std::make_unique<Io>();
        if (!reader->ov.hEvent || !writer->ov.hEvent) { result.error = GetLastError(); return result; }
        DWORD received = 0;
        BOOL ready = ReadFile(device.value, reader->buffer.data(), 64, &received, &reader->ov);
        DWORD read_error = ready ? 0 : GetLastError();
        if (!ready && read_error != ERROR_IO_PENDING) { result.error = read_error; return result; }
        writer->buffer = status_request();
        const auto deadline = GetTickCount64() + 1500;
        BOOL sent = WriteFile(device.value, writer->buffer.data(), 64, &result.written, &writer->ov);
        if (!complete_io(device.value, writer, sent, result.written, 500, result) || result.written != 64) {
            if (!ready) cancel_io(device.value, reader, result);
            return result;
        }
        while (result.reports < 64) {
            if (!ready) SetLastError(read_error);
            const auto now = GetTickCount64();
            const auto remaining = now < deadline ? static_cast<DWORD>(deadline - now) : 0;
            if (!complete_io(device.value, reader, ready, received, remaining, result)) return result;
            ++result.reports;
            auto status = parse_status(reader->buffer.data(), received);
            if (status.percentage_candidate >= 0) {
                result.status = status; result.read = received; result.reply = reader->buffer;
                return result;
            }
            if (GetTickCount64() >= deadline || result.reports == 64) {
                result.error = ERROR_TIMEOUT; result.timed_out = true; return result;
            }
            reader = std::make_unique<Io>(); received = 0;
            if (!reader->ov.hEvent) { result.error = GetLastError(); return result; }
            ready = ReadFile(device.value, reader->buffer.data(), 64, &received, &reader->ov);
            read_error = ready ? 0 : GetLastError();
        }
    } catch (...) { result.error = ERROR_UNHANDLED_EXCEPTION; }
    return result;
}
}
