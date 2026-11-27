import socket,subprocess,time,pathlib,os
root=pathlib.Path('/mnt/data/audit_work/www');root.mkdir(exist_ok=True);(root/'index.html').write_text('OK-TEST')
p=subprocess.Popen(['/mnt/data/audit_work/thttpd-master/thttpd','-D','-p','18976','-d',str(root),'-u','nobody','-nor'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
time.sleep(.5)
cases={'normal':(b'GET / HTTP/1.1\r\nHost: localhost\r\n\r\n',200),'duplicate_content_length':(b'POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 1\r\nContent-Length: 2\r\n\r\nX',400),'chunked':(b'POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n',400),'nul':(b'GET / HTTP/1.1\r\nHost: localhost\r\nX-A: a\x00b\r\n\r\n',400),'bad_length':(b'POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: -1\r\n\r\n',400),'folded':(b'GET / HTTP/1.1\r\nHost: localhost\r\nX-A: x\r\n y\r\n\r\n',400),'traversal':(b'GET /%2e%2e/etc/passwd HTTP/1.1\r\nHost: localhost\r\n\r\n',404)}
try:
 for name,(req,expected) in cases.items():
  try:
   with socket.create_connection(('127.0.0.1',18976),timeout=2) as s:
    s.sendall(req);s.settimeout(2);response=s.recv(8192)
   first=response.split(b'\r\n')[0];code=int(first.split()[1]);print(name,code,'PASS' if code==expected else 'FAIL',flush=True)
  except Exception as e:print(name,type(e).__name__,str(e),flush=True)
finally:p.terminate();p.wait(timeout=3)
