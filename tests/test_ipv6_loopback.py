import socket,subprocess,tempfile,time,os,signal
exe='/mnt/data/ipv6work/thttpd-master/thttpd'
with tempfile.TemporaryDirectory() as root:
 open(root+'/index.html','w').write('ipv6-ok\n')
 os.chmod(root,0o755)
 p=subprocess.Popen([exe,'-D','-p','18976','-d',root,'-u','root'],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
 try:
  time.sleep(.5)
  print('running',p.poll())
  for family,host,header in [(socket.AF_INET,'127.0.0.1','localhost'),(socket.AF_INET6,'::1','[::1]')]:
   try:
    s=socket.socket(family,socket.SOCK_STREAM);s.settimeout(2);s.connect((host,18976));s.sendall(f'GET / HTTP/1.1\r\nHost: {header}\r\n\r\n'.encode()); data=s.recv(8192); print(host,data[:90]);s.close()
   except Exception as e:print(host,type(e).__name__,str(e))
 finally:
  p.terminate()
  try:p.wait(timeout=3)
  except: p.kill()
  print('stderr',p.stderr.read()[-400:])
