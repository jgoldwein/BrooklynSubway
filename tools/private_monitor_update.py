#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Prepare a PRIVATE unit-bound monitoring OTA from committed source.

Contains a device credential: NEVER upload its output to GitHub. Builds in a
separate ignored project so the normal public build cache is never overwritten.
No account, ACL, device, eFuse, network or publication changes are performed.
"""
import argparse
import hashlib
import io
import json
import os
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path,PurePosixPath
import sign_firmware as signing

ROOT=Path(__file__).resolve().parents[1]
MARKER=b'BROOKLYN_PRIVATE_MONITOR_ENROLLMENT_V1'

def private_header(settings,unit):
    if not re.fullmatch(r'[0-9a-f]{16}',unit) or not 0<int(unit,16)<(1<<48):
        raise ValueError('Expected unit must be its exact 16-digit runtime board ID')
    import uuid
    if str(uuid.UUID(settings['deviceId'],version=4))!=settings['deviceId']:
        raise ValueError('Invalid canonical UUIDv4')
    for key,size in (('host',96),('password',64),('name',32)):
        value=settings[key]
        if not isinstance(value,str) or not 1<=len(value)<=size or value.strip()!=value or any(ord(c)<32 or ord(c)>126 for c in value):
            raise ValueError('Invalid bounded enrollment field: '+key)
    if len(settings['password'])<24 or not 1<=int(settings['port'])<=65535:
        raise ValueError('Invalid key length or TLS port')
    if not all(re.fullmatch(r'[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?',label) for label in settings['host'].split('.')):
        raise ValueError('Invalid DNS hostname')
    literal=lambda value:json.dumps(value,ensure_ascii=True)
    lines=['// PRIVATE GENERATED FILE - NEVER PUBLISH','#pragma once','#include <string.h>',
           'namespace brooklyn::monitor::private_seed {',f'constexpr uint64_t kExpectedUnit=0x{unit}ULL;',
           'inline Settings settings(){Settings s;']
    for key in ('host','deviceId','password','name'):
        lines.append(f'strcpy(s.{key},{literal(settings[key])});')
    lines.append(f's.port={int(settings["port"])};s.enabled=1;return s;}}\n}}\n')
    return '\n'.join(lines)

def extract_source(archive,destination):
    with zipfile.ZipFile(io.BytesIO(archive)) as zipped:
        for item in zipped.infolist():
            path=PurePosixPath(item.filename)
            if path.is_absolute() or Path(item.filename).drive or '..' in path.parts or '\\' in item.filename:
                raise ValueError('Unexpected source archive path')
            if item.filename in ('include/secrets.h','include/private_monitor_enrollment.h'):
                raise ValueError('Committed source contains a private credential/header')
        zipped.extractall(destination)

def prepare(out,settings,unit,archive,revision):
    header=private_header(settings,unit)
    out.mkdir(parents=True,exist_ok=False,mode=0o700)
    source=out/'private-source';source.mkdir(mode=0o700)
    extract_source(archive,source)
    (source/'include/private_monitor_enrollment.h').write_text(header,encoding='utf-8')
    info={'source_commit':revision,'expected_unit':unit,'device_id':settings['deviceId'],
          'private_contains_credential':True,'public_release_allowed':False}
    (out/'bundle.json').write_text(json.dumps(info,indent=2)+'\n',encoding='utf-8')
    (out/'READ_ME_PRIVATE.txt').write_text(
        'PRIVATE UNIT-SPECIFIC UPDATE - DO NOT POST TO GITHUB\n\n'
        'The firmware and private-source tree contain an individual monitoring key.\n'
        'The hardware ID check prevents applying the setup to a different unit; it\n'
        'does not encrypt the file. Share the signed .bkota privately with its owner.\n\n'
        'Install through that map\'s Advanced > Firmware updates > Manual signed package.\n'
        'No monitoring form entry is required. The matching broker account/ACL must\n'
        'already exist. Wi-Fi/admin/personalization/brightness settings are preserved.\n'
        'Future ordinary public updates preserve the installed monitoring record.\n'
        'Existing monitoring choices (including Off) are never overwritten.\n'
        'A non-secret applied marker prevents this same private image from silently\n'
        're-enrolling after credentials are removed or firmware defaults are reset.\n'
        'Installation is a one-time owner action; public automatic updates cannot\n'
        'securely transport a new per-unit credential to an unprovisioned device.\n',encoding='utf-8')
    return source

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--settings',required=True,type=Path)
    parser.add_argument('--expected-unit',required=True)
    parser.add_argument('--version',required=True)
    parser.add_argument('--private-key',type=Path)
    parser.add_argument('--key-id',type=int,default=1)
    parser.add_argument('--out',required=True,type=Path)
    parser.add_argument('--prepare-only',action='store_true')
    args=parser.parse_args()
    out=args.out.resolve()
    if not out.is_relative_to(ROOT/'dist/private-monitor'):
        parser.error('Private output must be under dist/private-monitor (git ignored)')
    if not re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+',args.version):parser.error('Invalid firmware version')
    if not args.prepare_only and not args.private_key:parser.error('--private-key is required to sign')
    settings=json.loads(args.settings.read_text(encoding='utf-8'))
    git=['git','-c','safe.directory='+ROOT.as_posix()]
    revision=subprocess.check_output(git+['rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    archive=subprocess.check_output(git+['archive','--format=zip','HEAD','platformio.ini','include','src','data','tools/build_version.py'],cwd=ROOT)
    source=prepare(out,settings,args.expected_unit,archive,revision)
    if args.prepare_only:
        print('Private source prepared under '+str(out));return 0
    child=os.environ.copy();child['BROOKLYN_FIRMWARE_VERSION']=args.version;child['BROOKLYN_PRIVATE_MONITOR_ENROLLMENT']='1'
    platformio=Path.home()/'.platformio/penv/Scripts/platformio.exe'
    with (out/'private-build.log').open('w',encoding='utf-8') as log:
        result=subprocess.run([str(platformio),'run','--project-dir',str(source),'-e','brooklyn-s3-network'],env=child,stdout=log,stderr=subprocess.STDOUT)
    if result.returncode:raise SystemExit('Private build failed; inspect the PRIVATE local build log (do not share it unreviewed)')
    payload=(source/'.pio/build/brooklyn-s3-network/firmware.bin').read_bytes()
    if MARKER not in payload:raise SystemExit('Private enrollment marker absent; refusing to label image preconfigured')
    key=signing.load_private_key(args.private_key)
    package=signing.build_package(payload,'brooklyn-s3',args.version,args.key_id,key)
    signing.verify_package(package,key.public_key())
    name=f'PRIVATE-brooklyn-s3-{args.version}.bkota'
    (out/name).write_bytes(package)
    (out/'SHA256SUMS.txt').write_text(hashlib.sha256(package).hexdigest()+' *'+name+'\n',encoding='ascii')
    print('PRIVATE signed update prepared: '+str(out/name))
    print('Contains a per-unit credential. Not uploaded or published.')
    return 0
if __name__=='__main__':raise SystemExit(main())
