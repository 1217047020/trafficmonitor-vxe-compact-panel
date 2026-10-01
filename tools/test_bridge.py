"""Isolated WS fixture; child-only registry override leaves the live host untouched."""
import base64
import hashlib
import http.server
import json
import pathlib
import socket
import struct
import subprocess
import threading
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]

sequence = [85, 'offline', 'stale', 'invalid', 0, 100, 'timeout', 85]
requests = []
lock = threading.Lock()

def read_exact(s, n):
    out = b''
    while len(out) < n:
        b = s.recv(n-len(out))
        if not b:
            raise ConnectionError('closed')
        out += b
    return out

class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def do_GET(self):
        self.connection.settimeout(3)
        key = self.headers['Sec-WebSocket-Key']
        accept = base64.b64encode(hashlib.sha1((key+'258EAFA5-E914-47DA-95CA-C5AB0DC85B11').encode()).digest()).decode()
        self.send_response(101)
        self.send_header('Upgrade','websocket')
        self.send_header('Connection','Upgrade')
        self.send_header('Sec-WebSocket-Accept', accept)
        self.end_headers()
        try:
            head = read_exact(self.connection,2)
            assert head[0] == 0x81 and head[1] & 0x80
            length=head[1]&127
            if length == 126: length=struct.unpack('!H',read_exact(self.connection,2))[0]
            assert length < 4096
            mask=read_exact(self.connection,4)
            data=read_exact(self.connection,length)
            query=json.loads(bytes(b^mask[i%4] for i,b in enumerate(data)))
            assert query == {'jsonrpc':'2.0','method':'devices.list','params':{},'id':1}
            with lock:
                index=len(requests)
                requests.append(query['method'])
            value=sequence[min(index,len(sequence)-1)]
            if value == 'timeout':
                time.sleep(6)
                return
            d={'id':'js:vxe/vxe-r1-nearlink:373B:10C9','connected':value!='offline',
               'stale':value=='stale','lastSeen':int(time.time()*1000),
               'percentage':None if value=='invalid' else value if isinstance(value,int) else 85}
            data=json.dumps({'jsonrpc':'2.0','id':1,'result':{'devices':[d]}}).encode()
            # Fragment every reply to validate accumulation of WS text fragments.
            for flag, fragment in [(0x01,data[:35]),(0x80,data[35:])]:
                n=len(fragment)
                header=bytes([flag,n]) if n < 126 else bytes([flag,126])+struct.pack('!H',n)
                self.connection.sendall(header+fragment)
        except (ConnectionError, socket.timeout, OSError):
            pass

server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler)
port=server.server_address[1]
threading.Thread(target=server.serve_forever,daemon=True).start()
exe=ROOT/'build/traffic-x64/bridge_probe.exe'
dll=ROOT/'build/traffic-x64/VxeBattery.dll'
process=subprocess.Popen([str(exe),str(dll),'38',str(port)],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
try:
    # Stop the listener after reconnect/recovery, then require fail-closed text.
    deadline=time.monotonic()+32
    while len(requests)<8 and time.monotonic()<deadline:
        time.sleep(0.2)
    server.shutdown()
    server.server_close()
    out,err=process.communicate(timeout=15)
    lines=out.decode('utf-8').splitlines()
    expected=['不可用','85%','离线','不可用','0%','100%','不可用','85%','不可用']
    assert process.returncode==0,err.decode(errors='replace')
    assert lines==expected,(lines,expected)
    assert len(requests)>=8,requests
    result={'passed':True,'states':lines,'rpcMethods':sorted(set(requests)),
            'dllSha256':hashlib.sha256(dll.read_bytes()).hexdigest(),
            'fragmentedReplies':True,'timeoutAndReconnect':True,'listenerStoppedFailClosed':True}
    (ROOT/'evidence/bridge-integration.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(result,ensure_ascii=False))
finally:
    if process.poll() is None: process.kill()
    server.server_close()
