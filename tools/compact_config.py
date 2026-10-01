"""Plan/apply/restore ONLY compact-panel display keys. No collection changes."""
import argparse
import json
from pathlib import Path
from traffic_config import get, put

DESIRED = {
    'tbar_display_item': '0',
    'plugin_display_item': 'vxe_compact_panel',
    'horizontal_arrange': 'false',  # full 32-DIP rectangle, not host's 21-DIP row
    'show_status_bar': 'false',
    'show_netspeed_figure': 'false',
}

def read(path):
    raw = Path(path).read_bytes()
    return raw, raw.decode('utf-8-sig')

def plan(path):
    raw, text = read(path)
    return {key: {'before': get(text, 'task_bar', key), 'after': value}
            for key, value in DESIRED.items()}

def transform(raw, changes, restore=False):
    text = raw.decode('utf-8-sig')
    for key, values in changes.items():
        if key not in DESIRED:
            raise ValueError('Unexpected key in compact backup')
        expected = values['after'] if restore else values['before']
        if get(text, 'task_bar', key) != expected:
            raise RuntimeError(f'Config changed since plan: [task_bar] {key}')
    for key, values in changes.items():
        text = put(text, 'task_bar', key, values['before' if restore else 'after'])
    return (b'\xef\xbb\xbf' if raw.startswith(b'\xef\xbb\xbf') else b'') + text.encode('utf-8')

def write(path, changes, restore=False):
    path = Path(path)
    raw = path.read_bytes()
    transformed = transform(raw, changes, restore)
    temp = path.with_name(path.name + '.compact-tmp')
    if temp.exists():
        raise RuntimeError('Temporary config already exists')
    temp.write_bytes(transformed)
    temp.replace(path)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('config', type=Path)
    parser.add_argument('action', choices=['plan', 'enable', 'restore'])
    parser.add_argument('--changes', type=Path)
    args = parser.parse_args()
    if args.action == 'plan':
        print(json.dumps(plan(args.config), ensure_ascii=False, indent=2))
    else:
        if not args.changes:
            parser.error('--changes is required')
        changes = json.loads(args.changes.read_text(encoding='utf-8-sig'))
        write(args.config, changes, args.action == 'restore')
