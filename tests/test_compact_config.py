from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from compact_config import plan, transform, write, DESIRED
from traffic_config import get, put

class CompactConfigTests(unittest.TestCase):
    def setUp(self):
        self.raw = (ROOT / 'tests/fixtures/traffic-config.ini').read_bytes()
        self.text = self.raw.decode('utf-8-sig')
        self.changes = plan(ROOT / 'tests/fixtures/traffic-config.ini')

    def test_round_trip_exact(self):
        for raw in [self.raw, b'\xef\xbb\xbf' + self.raw.lstrip(b'\xef\xbb\xbf')]:
            enabled = transform(raw, self.changes)
            self.assertEqual(transform(enabled, self.changes, True), raw)

    def test_only_named_display_keys(self):
        after = transform(self.raw, self.changes).decode('utf-8-sig')
        for key, value in DESIRED.items():
            self.assertEqual(get(after, 'task_bar', key), value)
        # This also proves all font, DPI/margin, acquisition, skin, history,
        # menus, colors and unrelated settings are byte-identical.
        def unrelated(text):
            for key in DESIRED:
                text = put(text, 'task_bar', key, None)
            return text
        self.assertEqual(unrelated(after), unrelated(self.text))

    def test_rollback_preserves_later_unrelated_settings(self):
        enabled = transform(self.raw, self.changes).decode('utf-8-sig')
        later = put(enabled, 'config', 'position_x', '1234')
        later = put(later, 'histroy_traffic', 'use_log_scale', 'false')
        restored = transform(later.encode('utf-8'), self.changes, True).decode('utf-8-sig')
        self.assertEqual(get(restored, 'config', 'position_x'), '1234')
        self.assertEqual(get(restored, 'histroy_traffic', 'use_log_scale'), 'false')

    def test_conflict_refuses_overwrite(self):
        enabled = transform(self.raw, self.changes).decode('utf-8-sig')
        conflict = put(enabled, 'task_bar', 'plugin_display_item', 'user_other_plugin')
        with self.assertRaisesRegex(RuntimeError, 'Config changed'):
            transform(conflict.encode('utf-8'), self.changes, True)

    def test_invalid_backup_refused(self):
        with self.assertRaises(ValueError):
            transform(self.raw, {'font_size': {'before': '9', 'after': '8'}})

if __name__ == '__main__':
    unittest.main()
