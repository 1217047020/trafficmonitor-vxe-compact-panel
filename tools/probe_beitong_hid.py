"""Metadata only for the two KP20 receiver identities; NEVER sends HID reports."""
import ctypes as C
import importlib.util
import json
from pathlib import Path
from datetime import datetime, timezone
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('existing_hid_probe',root/'tools/probe_hid.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
lib=m.load_api()
rows=[]
try:
    for vid in (0x20BC,0x20DD):
        head=lib.hid_enumerate(vid,0x5159)
        try:
            current=head
            while current:
                info=current.contents
                row={'vid':info.vendor_id,'pid':info.product_id,
                     'product':info.product_string,'interface':info.interface_number,
                     'usagePage':info.usage_page,'usage':info.usage}
                try:row['caps']=m.report_caps(info.path)
                except Exception as e:row['capsError']=str(e)
                handle=lib.hid_open_path(info.path)
                if handle:
                    try:
                        buffer=(C.c_ubyte*4096)()
                        count=lib.hid_get_report_descriptor(handle,buffer,len(buffer))
                        if count>0:row['reportDescriptorHex']=bytes(buffer[:count]).hex(' ')
                        else:row['descriptorError']='descriptor getter returned '+str(count)
                    finally:lib.hid_close(handle)
                else:row['descriptorError']='metadata handle not available'
                rows.append(row);current=info.next
        finally:lib.hid_free_enumeration(head)
finally:lib.hid_exit()
print(json.dumps({'readOnly':True,'capturedAt':datetime.now(timezone.utc).isoformat(),'interfaces':rows},ensure_ascii=False,indent=2))
