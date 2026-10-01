"""Edit only named TrafficMonitor INI settings, preserving unrelated lines/BOM."""
import argparse
import json
import pathlib
import re

ITEM = 'vxe_r1_battery'
FIELDS = {'config': ['show_task_bar_wnd','hide_main_window','skin_selected','lock_window_pos',
                     'mouse_penetrate','always_on_top','plugin_disabled'],
          'task_bar': ['plugin_display_item']}

def get(text, section, key):
    active = False
    for line in text.splitlines():
        if re.match(r'^\s*\[.*\]\s*$',line): active = line.strip()==f'[{section}]'
        elif active:
            m=re.match(r'^\s*'+re.escape(key)+r'\s*=\s*(.*)$',line)
            if m: return m[1]
    return None

def put(text, section, key, value):
    lines=text.splitlines(keepends=True)
    active=False
    insertion=None
    for i,line in enumerate(lines):
        if re.match(r'^\s*\[.*\]\s*$',line):
            if active: insertion=i; break
            active=line.strip()==f'[{section}]'
            if active: insertion=i+1
        elif active:
            insertion=i+1
            m=re.match(r'^([ \t]*'+re.escape(key)+r'[ \t]*=[ \t]*)[^\r\n]*(\r?\n)?$',line)
            if m:
                if value is None: del lines[i]
                else: lines[i]=m[1]+value+(m[2] or '')
                return ''.join(lines)
    if value is not None:
        if insertion is None:
            if lines and not lines[-1].endswith('\n'): lines[-1]+='\r\n'
            lines.extend([f'[{section}]\r\n',f'{key} = {value}\r\n'])
        else: lines.insert(insertion,f'{key} = {value}\r\n')
    return ''.join(lines)

def configure(path, action, backup):
    raw=path.read_bytes()
    bom=raw.startswith(b'\xef\xbb\xbf')
    text=raw.decode('utf-8-sig')
    if action=='install':
        if backup.exists(): raise RuntimeError('Config backup already exists')
        saved={s:{k:get(text,s,k) for k in keys} for s,keys in FIELDS.items()}
        backup.write_text(json.dumps(saved,ensure_ascii=False,indent=2),encoding='utf-8')
    else: saved=json.loads(backup.read_text(encoding='utf-8'))
    if action=='restore':
        for s,keys in saved.items():
            for k,v in keys.items(): text=put(text,s,k,v)
    else:
        items=[x for x in (get(text,'task_bar','plugin_display_item') or '').split(',') if x and x!=ITEM]
        if action in ('install','taskbar'): items.append(ITEM)
        text=put(text,'task_bar','plugin_display_item',','.join(items))
        disabled=[x for x in (get(text,'config','plugin_disabled') or '').split(',') if x and x.lower()!='vxebattery.dll']
        text=put(text,'config','plugin_disabled',','.join(disabled))
        text=put(text,'config','show_task_bar_wnd','true')
        if action=='desktop':
            settings={'hide_main_window':'0','skin_selected':'VXE Battery','lock_window_pos':'true',
                      'mouse_penetrate':'true','always_on_top':'false'}
            for k,v in settings.items(): text=put(text,'config',k,v)
        elif action=='taskbar':
            for k in ['hide_main_window','skin_selected','lock_window_pos','mouse_penetrate','always_on_top']:
                text=put(text,'config',k,saved['config'][k])
    data=text.encode('utf-8')
    if bom: data=b'\xef\xbb\xbf'+data
    temp=path.with_name(path.name+'.vxe-tmp')
    temp.write_bytes(data)
    temp.replace(path)

if __name__=='__main__':
    p=argparse.ArgumentParser()
    p.add_argument('path',type=pathlib.Path)
    p.add_argument('action',choices=['install','taskbar','desktop','restore'])
    p.add_argument('backup',type=pathlib.Path)
    a=p.parse_args()
    configure(a.path,a.action,a.backup)
