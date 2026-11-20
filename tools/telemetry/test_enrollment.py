# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
import json
import tempfile
from pathlib import Path
import enroll

def main():
    with tempfile.TemporaryDirectory() as temp:
        base=Path(temp)
        a=enroll.prepare(base/'a','Avi map')
        b=enroll.prepare(base/'b','Test map')
        sa=json.loads((base/'a'/'device-settings.json').read_text())
        sb=json.loads((base/'b'/'device-settings.json').read_text())
        assert sa['password']!=sb['password'] and len(sa['password'])==48
        assert a['device_id']!=b['device_id'] and a['username']=='bkmap-'+sa['deviceId']
        assert sa['enabled']==1 and sa['port']==8884
        acl=(base/'a'/'broker-acl.fragment').read_text()
        assert 'read' not in acl and '#' not in acl and '+' not in acl
        assert len(acl.splitlines())==4 and all(topic in acl for topic in a['topics'])
        assert sa['password'] not in json.dumps(a) and sa['password'] not in acl
        assert sa['password'] not in (base/'a'/'SETUP.txt').read_text()
        try:enroll.prepare(base/'a','Again');raise AssertionError('overwrote credentials')
        except FileExistsError:pass
        for host in ('mqtt://host','x..com','x-.com','-x.com','x.com.','x'*64+'.com'):
            try:enroll.prepare(base/'invalid','Invalid',host);raise AssertionError('accepted invalid host')
            except ValueError:pass
        assert not (base/'invalid').exists()
    print('Enrollment: unique credentials, confined ACLs, default enabled, no overwrite, no secret output PASS')
if __name__=='__main__':main()
