"""Scoped KP20D status capture. Default passive; --query sends one 02 15 status request.

Never performs synchronization, configuration, vibration, factory or firmware commands.
Only VID20BC/PID5159, interface1, usageFF/3, 64-byte I/O reports are allowed.
"""
import argparse
import ctypes as C
import json
import time
from datetime import datetime, timezone
from pathlib import Path

from probe_hid import load_api, report_caps
from beitong_battery_protocol import parse_status, status_request


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--query', action='store_true')
    parser.add_argument('--seconds', type=float, default=3)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if not 0.1 <= args.seconds <= 10:
        parser.error('--seconds must be between 0.1 and 10')
    result = {'capturedAt': datetime.now(timezone.utc).isoformat(),
              'mode': 'status_query' if args.query else 'passive',
              'hardwareComparisonPassed': False, 'reports': [], 'reportCount': 0}
    lib = load_api()
    head = None
    handle = None
    try:
        head = lib.hid_enumerate(0x20BC, 0x5159)
        paths = []
        node = head
        while node:
            info = node.contents
            if (info.interface_number, info.usage_page, info.usage) == (1, 0xFF, 3):
                paths.append(info.path)
            node = info.next
        if len(paths) != 1:
            raise RuntimeError('Expected exactly one KP20D vendor status collection')
        caps = report_caps(paths[0])
        if (caps['input_length'], caps['output_length'], caps['feature_length']) != (64, 64, 0):
            raise RuntimeError('Unexpected report lengths; refusing device access')
        result['caps'] = caps
        handle = lib.hid_open_path(paths[0])
        if not handle:
            raise RuntimeError('Cannot open vendor status collection')
        if args.query:
            request = status_request()
            result['requestHex'] = request.hex(' ')
            n = lib.hid_write(handle, C.create_string_buffer(request), len(request))
            result['written'] = n
            if n != len(request):
                raise RuntimeError('Status request write did not complete')
        deadline = time.monotonic() + args.seconds
        seen = set()
        buffer = C.create_string_buffer(64)
        while time.monotonic() < deadline and result['reportCount'] < 2000:
            n = lib.hid_read_timeout(handle, buffer, len(buffer), 100)
            if n < 0:
                raise RuntimeError('Vendor input report read failed')
            if not n:
                continue
            result['reportCount'] += 1
            report = buffer.raw[:n]
            # Save only unique status reports and a header histogram; omit
            # arbitrary device/key/memory payloads from evidence.
            header = report[:2].hex(' ')
            counts = result.setdefault('headerCounts', {})
            counts[header] = counts.get(header, 0) + 1
            parsed = parse_status(report)
            if parsed and report not in seen:
                seen.add(report)
                result['reports'].append({'receivedAfterMs': round((time.monotonic() - (deadline - args.seconds))*1000),
                                          'responseHex': report.hex(' '), **parsed})
        result['status'] = 'status_received' if result['reports'] else 'no_valid_status'
    except Exception as exc:
        result['status'] = 'error'
        result['error'] = str(exc)
    finally:
        if handle:
            lib.hid_close(handle)
        if head:
            lib.hid_free_enumeration(head)
        lib.hid_exit()
    rendered = json.dumps(result, indent=2, ensure_ascii=False)
    if args.output:
        args.output.write_text(rendered + '\n', encoding='utf-8')
    print(rendered)


if __name__ == '__main__':
    main()
