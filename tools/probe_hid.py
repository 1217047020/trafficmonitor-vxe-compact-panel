"""Scoped HID diagnostic. Default is enumeration; --query sends ONLY battery 0x04.

Uses the verified BatteryMonitor release's hidapi DLL; no pip dependencies.
HID paths/serials are deliberately excluded from saved evidence.
"""
import argparse
import ctypes as C
from ctypes import wintypes as W
import hashlib
import json
import re
from pathlib import Path
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[1]
VID, PID = 0x373B, 0x10C9


class Info(C.Structure):
    pass


Info._fields_ = [
    ('path', C.c_char_p), ('vendor_id', C.c_ushort), ('product_id', C.c_ushort),
    ('serial_number', C.c_wchar_p), ('release_number', C.c_ushort),
    ('manufacturer_string', C.c_wchar_p), ('product_string', C.c_wchar_p),
    ('usage_page', C.c_ushort), ('usage', C.c_ushort), ('interface_number', C.c_int),
    ('next', C.POINTER(Info)), ('bus_type', C.c_int),
]


class Caps(C.Structure):
    _fields_ = [('usage', W.USHORT), ('usage_page', W.USHORT),
                ('input_length', W.USHORT), ('output_length', W.USHORT),
                ('feature_length', W.USHORT), ('reserved', W.USHORT * 17),
                ('counts', W.USHORT * 10)]


def report_caps(path):
    kernel = C.WinDLL('kernel32', use_last_error=True)
    hid = C.WinDLL('hid')
    kernel.CreateFileW.argtypes = [W.LPCWSTR, W.DWORD, W.DWORD, C.c_void_p,
                                  W.DWORD, W.DWORD, W.HANDLE]
    kernel.CreateFileW.restype = W.HANDLE
    kernel.CloseHandle.argtypes = [W.HANDLE]
    hid.HidD_GetPreparsedData.argtypes = [W.HANDLE, C.POINTER(C.c_void_p)]
    hid.HidD_FreePreparsedData.argtypes = [C.c_void_p]
    hid.HidP_GetCaps.argtypes = [C.c_void_p, C.POINTER(Caps)]
    h = kernel.CreateFileW(path.decode(), 0, 3, None, 3, 0, None)
    if h == C.c_void_p(-1).value:
        raise C.WinError(C.get_last_error())
    pp = C.c_void_p()
    try:
        if not hid.HidD_GetPreparsedData(h, C.byref(pp)):
            raise RuntimeError('HidD_GetPreparsedData failed')
        caps = Caps()
        if hid.HidP_GetCaps(pp, C.byref(caps)) != 0x110000:
            raise RuntimeError('HidP_GetCaps failed')
        return {k: getattr(caps, k) for k in
                ['usage', 'usage_page', 'input_length', 'output_length', 'feature_length']}
    finally:
        if pp:
            hid.HidD_FreePreparsedData(pp)
        kernel.CloseHandle(h)


def load_api():
    lib = C.CDLL(str(ROOT / 'runtime/hidapi.dll'))
    signatures = {
        'hid_init': ([], C.c_int), 'hid_exit': ([], C.c_int),
        'hid_enumerate': ([C.c_ushort, C.c_ushort], C.POINTER(Info)),
        'hid_free_enumeration': ([C.POINTER(Info)], None),
        'hid_open_path': ([C.c_char_p], C.c_void_p),
        'hid_close': ([C.c_void_p], None),
        'hid_write': ([C.c_void_p, C.c_void_p, C.c_size_t], C.c_int),
        'hid_read_timeout': ([C.c_void_p, C.c_void_p, C.c_size_t, C.c_int], C.c_int),
        'hid_error': ([C.c_void_p], C.c_wchar_p),
        'hid_get_report_descriptor': ([C.c_void_p, C.c_void_p, C.c_size_t], C.c_int),
    }
    for name, (args, restype) in signatures.items():
        fn = getattr(lib, name)
        fn.argtypes, fn.restype = args, restype
    if lib.hid_init() != 0:
        raise RuntimeError('hid_init failed')
    return lib


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--query', action='store_true')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    lib = load_api()
    head = lib.hid_enumerate(VID, PID)
    result = {'timestamp_utc': datetime.now(timezone.utc).isoformat(),
              'vid': '373B', 'pid': '10C9', 'mode': 'query' if args.query else 'enumerate',
              'interfaces': []}
    try:
        node = head
        while node:
            info = node.contents
            path = info.path
            item = {'interface': info.interface_number, 'usage_page': info.usage_page,
                    'usage': info.usage, 'product': info.product_string,
                    'path_id': hashlib.sha256(path).hexdigest()[:12]}
            collection = re.search(rb'&col([0-9a-f]+)#', path, re.I)
            item['collection'] = collection.group(1).decode() if collection else None
            result['interfaces'].append(item)
            try:
                item['caps'] = report_caps(path)
                if info.usage_page in (0xFF02, 0xFF04):
                    h = lib.hid_open_path(path)
                    if not h:
                        raise RuntimeError(lib.hid_error(None) or 'open failed')
                    try:
                        descriptor = C.create_string_buffer(4096)
                        dn = lib.hid_get_report_descriptor(h, descriptor, len(descriptor))
                        if dn > 0:
                            item['descriptor_hex'] = descriptor.raw[:dn].hex(' ')
                        if (not args.query or item['caps']['input_length'] != 17
                                or item['caps']['output_length'] != 17):
                            node = info.next
                            continue
                        # Exact upstream query; Windows hidapi pads to output report length.
                        packet = bytes([0x08, 0x04] + [0] * 14 + [0x49])
                        item['request_hex'] = packet.hex(' ')
                        item['written'] = lib.hid_write(h, C.create_string_buffer(packet), len(packet))
                        if item['written'] < 0:
                            raise RuntimeError(lib.hid_error(h) or 'write failed')
                        buf = C.create_string_buffer(max(64, item['caps']['input_length']))
                        n = lib.hid_read_timeout(h, buf, len(buf), 500)
                        if n < 0:
                            raise RuntimeError(lib.hid_error(h) or 'read failed')
                        item['response_hex'] = buf.raw[:n].hex(' ')
                        item['response_length'] = n
                        item['status'] = 'response' if n else 'timeout'
                    finally:
                        lib.hid_close(h)
            except Exception as exc:
                item['error'] = str(exc)
            node = info.next
    finally:
        if head:
            lib.hid_free_enumeration(head)
        lib.hid_exit()
    rendered = json.dumps(result, indent=2, ensure_ascii=False)
    print(rendered)
    if args.output:
        args.output.write_text(rendered + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
