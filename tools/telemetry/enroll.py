#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Prepare one private enrollment bundle. Does NOT change a broker or device."""
import argparse
import json
import os
import re
import secrets
import uuid
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]

def prepare(out,name,host='mqtt.goldwein.net',port=8884):
    if not 1<=len(name)<=32 or any(ord(c)<32 or ord(c)>126 for c in name) or name.strip()!=name:
        raise ValueError('Name must be 1-32 printable ASCII characters, without outer spaces')
    if len(host)>96 or not all(re.fullmatch(r'[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?',label) for label in host.split('.')):
        raise ValueError('Use a DNS hostname, without a URL, path or port')
    if not 1<=port<=65535:raise ValueError('Invalid TLS port')
    out=Path(out)
    out.mkdir(parents=True,exist_ok=False,mode=0o700)
    device=str(uuid.uuid4());username='bkmap-'+device
    settings={'host':host,'port':port,'deviceId':device,'name':name,'enabled':1,'password':secrets.token_hex(24)}
    # Password never printed, never passed as a process argument. POSIX mode 0600;
    # Windows inherits the containing directory's ACL, so keep the directory private.
    fd=os.open(out/'device-settings.json',os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
    with os.fdopen(fd,'w',encoding='utf-8') as stream:json.dump(settings,stream,indent=2);stream.write('\n')
    topics=[f'brooklyn/v1/{username}/{kind}' for kind in ('status','event','availability')]
    (out/'broker-acl.fragment').write_text('user '+username+'\n'+''.join('topic write '+t+'\n' for t in topics),encoding='ascii')
    (out/'SETUP.txt').write_text(
        f'PRIVATE MONITORING ENROLLMENT: {name}\n\n'
        f'MQTT username: {username}\nDevice ID: {device}\nTLS server: {host}:{port}\n\n'
        'device-settings.json contains the private monitoring key. Do not publish, email broadly, or commit it.\n'
        'The device and the broker must use the SAME key.\n\n'
        'WHERE THESE FILES BELONG:\n'
        'This bundle stays on the computer where you generated it (for example, Windows).\n'
        'Open device-settings.json there to read the key. Do not copy the JSON file into /etc/mosquitto.\n'
        'The following account and ACL changes happen in the Linux server terminal.\n\n'
        'BROKER ADMINISTRATOR (existing Mosquitto password/ACL files):\n'
        '1. Back up the existing password and ACL files; preserve their owners and modes.\n'
        f'2. Run: sudo mosquitto_passwd /etc/mosquitto/passwd {username}\n'
        '   Paste the password value from device-settings.json at both hidden prompts. Do not use -c or -b.\n'
        '3. Append broker-acl.fragment to the 8884 listener ACL, preserving every existing rule.\n'
        '   vi/vim: Esc, :wq, Enter saves and exits. nano: Ctrl+O, Enter, Ctrl+X.\n'
        '   Editor lines containing only ~ are not part of the ACL.\n'
        '4. Reload Mosquitto and check its journal/log for errors.\n\n'
        'WARNINGS:\n'
        'Newer GNU cp may warn about -n; cp -p --update=none avoids replacing an existing backup.\n'
        'A future-ownership warning from mosquitto_passwd is not itself a failed update.\n'
        'Inspect file metadata and broker read access before changing live ownership/permissions.\n\n'
        'MAP ADMINISTRATOR:\n'
        '1. Install the monitoring firmware. Open Advanced > Remote monitoring and sign in.\n'
        '2. Copy the server, TLS port, Device ID, map label and password from device-settings.json.\n'
        '3. Leave On selected and press Save monitoring. No reboot is needed.\n'
        '4. Confirm a server acknowledgment and a CURRENT status message using your read-authorized MQTT client.\n'
        '   A broker acknowledgment alone does not prove ACL delivery.\n'
        '5. Keep the bundle private for recovery; revoke the server account if its key is exposed.\n\n'
        'A UUID is NOT a secret. The password is. Never use one bundle on two maps at once.\n',encoding='utf-8')
    return {'directory':str(out.resolve()),'username':username,'device_id':device,'topics':topics}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--name',required=True)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--host',default='mqtt.goldwein.net')
    parser.add_argument('--port',type=int,default=8884)
    args=parser.parse_args()
    try:result=prepare(args.out,args.name,args.host,args.port)
    except (ValueError,OSError) as error:parser.error(str(error))
    print(json.dumps(result,indent=2))
if __name__=='__main__':main()
