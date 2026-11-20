#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Credential-safe MQTT 5 broker ACL preflight; never sends a device heartbeat.

MQTT 3.1.1 PUBACK alone cannot prove write authorization. Mosquitto 2.0's MQTT 5
PUBACK reason codes allow this separate diagnostic client to check authorization.
Two non-retained diagnostic events are attempted: own namespace and a random
foreign namespace. Normal collectors reject the diagnostic (not a schema event).
No subscriptions, commands, retained messages, account/ACL changes or passwords
on the command line. This checks the broker, not actual device/collector delivery.
"""
import argparse
import json
import secrets
import socket
import ssl
import struct
import uuid
from pathlib import Path

def string(text):
    value=text.encode('utf-8')
    if len(value)>65535:raise ValueError('MQTT string too long')
    return struct.pack('!H',len(value))+value

def packet(kind,payload):
    n=len(payload);length=bytearray()
    if n>4096:raise ValueError('Diagnostic packet too large')
    while True:
        digit=n%128;n//=128;length.append(digit|(128 if n else 0))
        if not n:break
    return bytes([kind])+length+payload

def exact(sock,size):
    data=bytearray()
    while len(data)<size:
        part=sock.recv(size-len(data))
        if not part:raise ConnectionError('Broker closed connection')
        data.extend(part)
    return bytes(data)

def receive(sock):
    kind=exact(sock,1)[0];length=0
    for offset in range(4):
        digit=exact(sock,1)[0];length+=(digit&127)<<(offset*7)
        if length>4096:raise ValueError('Broker packet exceeds diagnostic limit')
        if not digit&128:return kind,exact(sock,length)
    raise ValueError('Invalid broker remaining length')

def check(settings):
    device=settings['deviceId']
    if str(uuid.UUID(device,version=4))!=device:raise ValueError('Invalid canonical device UUID')
    username='bkmap-'+device
    foreign='bkmap-'+str(uuid.uuid4())
    connect=string('MQTT')+bytes([5,0xc2])+struct.pack('!H',30)+b'\0'+string('bkcheck-'+secrets.token_hex(8))+string(username)+string(settings['password'])
    with socket.create_connection((settings['host'],int(settings['port'])),timeout=8) as tcp:
        with ssl.create_default_context().wrap_socket(tcp,server_hostname=settings['host']) as tls:
            tls.settimeout(8);tls.sendall(packet(0x10,connect));kind,data=receive(tls)
            if kind!=0x20 or len(data)<3 or data[1]!=0:raise ValueError('MQTT authentication was not accepted')
            codes=[]
            for mid,user in enumerate((username,foreign),1):
                topic=f'brooklyn/v1/{user}/event'
                payload=b'{"diagnostic":"ACL preflight; not a device heartbeat or event"}'
                tls.sendall(packet(0x32,string(topic)+struct.pack('!H',mid)+b'\0'+payload))
                kind,data=receive(tls)
                if kind!=0x40 or len(data)<2 or data[:2]!=struct.pack('!H',mid):raise ValueError('Unexpected MQTT acknowledgement')
                codes.append(data[2] if len(data)>2 else 0)
            tls.sendall(b'\xe0\0')
            return {'verified_tls':tls.version(),'account_accepted':True,'own_event_publish_allowed':codes[0] in (0,16),
                    'foreign_event_publish_denied':codes[1]==135,'mqtt5_puback_codes':codes,
                    'device_delivery_observed':False,'retained_messages_sent':False}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--settings',required=True,type=Path)
    args=parser.parse_args()
    try:result=check(json.loads(args.settings.read_text(encoding='utf-8')))
    except Exception as error:
        # Exception strings from networking libraries can contain remote details;
        # keep diagnostic output categoric and credential-free.
        print(json.dumps({'preflight_passed':False,'error_type':type(error).__name__}));return 1
    result['preflight_passed']=result['own_event_publish_allowed'] and result['foreign_event_publish_denied']
    print(json.dumps(result,indent=2));return 0 if result['preflight_passed'] else 1
if __name__=='__main__':raise SystemExit(main())
