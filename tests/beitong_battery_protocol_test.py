"""Offline only: no DLL loading, enumeration, or hardware access."""
import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from beitong_battery_protocol import parse_status, status_request


class StatusTests(unittest.TestCase):
    def test_request(self):
        self.assertEqual(status_request(), b'\x02\x15' + bytes(62))

    def test_captured_discharge_report(self):
        report = bytes.fromhex('02 15 3f 00 50 01 00') + bytes(57)
        self.assertEqual(parse_status(report)['percentageCandidate'], 63)
        self.assertEqual(parse_status(report)['chargeStateRaw'], 0)

    def test_percentage_and_charge_nibble(self):
        for percentage in (0, 1, 50, 100):
            frame = bytearray(status_request())
            frame[2], frame[4] = percentage, 0xA3
            status = parse_status(frame)
            self.assertEqual(status['percentageCandidate'], percentage)
            self.assertEqual(status['chargeStateRaw'], 3)

    def test_reject_other_frames(self):
        self.assertIsNone(parse_status(b''))
        self.assertIsNone(parse_status(status_request()[:-1]))
        self.assertIsNone(parse_status(status_request() + b'\0'))
        for offset, value in ((0, 1), (1, 0x55), (2, 101), (2, 255)):
            frame = bytearray(status_request())
            frame[offset] = value
            self.assertIsNone(parse_status(frame))


if __name__ == '__main__':
    unittest.main()
