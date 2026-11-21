#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Bounded diagnostics, restart intent, outage policy and ingestion regression tests."""
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import phase4e_host as h
sys.path.insert(0,str(h.ROOT))
from tools.telemetry.schema_validate import load_schema,validate
from tools.telemetry.ingest import Collector
from tools.telemetry.store import Store
from tools.telemetry.test_telemetry import status
from tools.telemetry.ha_diagnostics import extend_discovery

def main():
    with tempfile.TemporaryDirectory() as tmp:
        exe=Path(tmp)/'remote_diagnostics.exe'
        sources=['tools/host_tests/remote_diagnostics_test.cpp','src/telemetry_core.cpp','src/wan_address.cpp','src/monitor_config.cpp']
        cmd=h.compiler()+['-std=c++17','-Wall','-Wextra','-Werror','-O1','-fno-exceptions',f'-I{h.ROOT / "include"}',*[str(h.ROOT/s) for s in sources],'-o',str(exe)]
        built=subprocess.run(cmd,capture_output=True,text=True)
        if built.returncode:print(built.stdout+built.stderr);return 1
        run=subprocess.run([str(exe)],capture_output=True,text=True)
        docs=[]
        for line in run.stdout.splitlines():
            if line.startswith('DIAGNOSTICS '):docs.append(json.loads(line[12:]))
            else:print(line)
        if run.returncode:print(run.stderr);return run.returncode
    schema=load_schema('brooklyn-diagnostics-v1.schema.json')
    assert len(docs)==3
    for d in docs:assert not validate(d,schema),validate(d,schema)
    base=docs[1];device=base['device_id'];boot=base['boot_id'];topic=f'brooklyn/v1/bkmap-{device}/event'
    store=Store();collector=Collector(store,lambda:1800000010)
    send=lambda d,retained=False,t=topic:collector.handle(t,json.dumps(d).encode(),retained)
    assert send(base)=='stale_diagnostics_boot'
    assert collector.handle(f'brooklyn/v1/bkmap-{device}/status',json.dumps(status(device=device,boot=boot)).encode())=='status'
    assert send(base)=='diagnostics'
    assert send(base)=='duplicate_or_old'
    assert send(docs[0])=='duplicate_or_old'
    assert send(base,True)=='rejected:retained_diagnostics'
    assert store.get_device(device)['sequence']==1 # diagnostics never advances heartbeat/freshness
    assert store.get_diagnostics(device)['sequence']==4
    bad=copy.deepcopy(base);bad['password']='do not accept';assert send(bad)=='rejected:schema'
    bad=copy.deepcopy(base);bad['device_id']='22222222-2222-4222-8222-222222222222';assert send(bad)=='rejected:device_id_mismatch'
    bad=copy.deepcopy(base);bad['boot_id']='bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb';assert send(bad)=='stale_diagnostics_boot'
    bad=copy.deepcopy(base);bad['time_synced']=False;assert send(bad)=='rejected:schema'
    # Only one latest snapshot per map, not a minute-by-minute activity archive.
    for i in range(5,55):
        d=copy.deepcopy(base);d['sequence']=i;assert send(d)=='diagnostics'
    assert store.db.execute('select count(*) from diagnostics_current').fetchone()[0]==1
    assert store.db.execute('select count(*) from events').fetchone()[0]==0
    original={'components':{'existing':{'platform':'sensor','name':'Existing metric'}}}
    extended=extend_discovery(original,device,'trial_subway')
    assert original=={'components':{'existing':{'platform':'sensor','name':'Existing metric'}}}
    assert extended['components']['existing']==original['components']['existing']
    for key,c in extended['components'].items():
        if key=='existing':continue
        assert c['entity_category']=='diagnostic'
        assert c['state_topic']==(topic.replace('/event','/status') if key=='map_name' else topic)
        assert 'command_topic' not in c
        if key=='map_name':assert 'device_name' in c['value_template']
        else:assert "'diagnostics'" in c['value_template'] and device in c['value_template']
    assert len(extended['components'])==29
    try:extend_discovery(original,device,'bad/namespace')
    except ValueError:pass
    else:raise AssertionError('unsafe prefix accepted')
    print('Diagnostics schema/privacy/identity/replay/latest-only storage: 66 checks PASS')
    print('Home Assistant: 28 diagnostic components including map name; existing configuration preserved; no command topics PASS')
    return 0
if __name__=='__main__':raise SystemExit(main())
