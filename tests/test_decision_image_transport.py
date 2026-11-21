#!/usr/bin/env python3
"""Resident image envelope, preserving independent image and typed JSON bounds."""
import hashlib
import os
import select
import struct
import subprocess
import sys
from test_decision_transport import receive


def frame(rgba,width,height,request):
    return b'SMIMG001'+struct.pack('!IIII',width,height,len(rgba),len(request))+rgba+request


def main():
    binary=sys.argv[1];checks=0
    def start():return subprocess.Popen([binary,'--image-frame-fixture'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,bufsize=0)
    def send(p,data,fragment=8192):
        offset=0
        while offset<len(data):
            assert select.select([], [p.stdin], [], 10)[1]
            n=os.write(p.stdin.fileno(),data[offset:offset+fragment]);assert n>0;offset+=n
    p=start()
    try:
        cases=[(1,1,b'\xff\x00\x00\x01',b'{"state":"blue"}'),(2,1,b'\x00\xff\x00\xff'*2,'🙂'.encode()),
            (1,3,b'\x00\x00\xff\x00'*3,b'\0binary\0'),(512,512,bytes(range(256))*4096,b'x'*1048576)]
        for i,(w,h,rgba,request) in enumerate(cases):
            send(p,frame(rgba,w,h,request),1 if i<2 else 8192);r=receive(p)
            assert r==dict(width=w,height=h,image_bytes=len(rgba),request_bytes=len(request),image_sha256=hashlib.sha256(rgba).hexdigest(),request_sha256=hashlib.sha256(request).hexdigest())
            assert p.poll() is None;checks+=1
        for request,error in [(b'throw','engine_failed'),(b'empty','engine_failed'),(b'oversize','decision_result_limit'),(b'refuse','context_limit')]:
            send(p,frame(bytes(4),1,1,request));assert receive(p)==dict(error=error)
            send(p,frame(bytes(4),1,1,b'recovered'));assert receive(p)['request_bytes']==9;assert p.poll() is None;checks+=1
        p.stdin.close();assert p.wait(timeout=5)==0;checks+=1
    finally:
        if p.poll() is None:p.kill();p.wait()
        p.stdout.close();p.stderr.close()
    bad=[b'x',b'SMIMG000'+struct.pack('!IIII',1,1,4,1),
        b'SMIMG001'+struct.pack('!IIII',0,1,0,1),b'SMIMG001'+struct.pack('!IIII',513,1,2052,1),
        b'SMIMG001'+struct.pack('!IIII',1,0,0,1),b'SMIMG001'+struct.pack('!IIII',1,513,2052,1),
        b'SMIMG001'+struct.pack('!IIII',1,1,3,1),b'SMIMG001'+struct.pack('!IIII',1,1,4,0),
        b'SMIMG001'+struct.pack('!IIII',1,1,4,1048577),frame(bytes(4),1,1,b'abc')[:-1],
        frame(bytes(4),1,1,b'abc')[:25]]
    for data in bad:
        p=start();p.stdin.write(data);p.stdin.close();assert p.wait(timeout=5)!=0;assert not p.stdout.read();p.stdout.close();p.stderr.close();checks+=1
    # Broken readers fail without exposing partial successful replies or paths.
    p=start();p.stdout.close()
    try:
        send(p,frame(bytes(4),1,1,b'closed'));p.stdin.close();assert p.wait(timeout=5)!=0;checks+=1
    finally:
        if p.poll() is None:p.kill();p.wait()
        p.stderr.close()
    print(f'PASS resident decision image transport: {checks} checks; changing frames, independent full-size limits, refusal recovery, fragmentation, truncation and EOF')


if __name__=='__main__':main()
