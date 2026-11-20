# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
import io
import json
import tempfile
import unittest
import zipfile
from pathlib import Path
import private_monitor_update as private

def archive(extra=None):
    out=io.BytesIO()
    with zipfile.ZipFile(out,'w') as z:
        z.writestr('include/monitor_config.h','// fixture')
        z.writestr('platformio.ini','[platformio]')
        if extra:z.writestr(extra,'not allowed')
    return out.getvalue()

class PrivateUpdateTests(unittest.TestCase):
    def settings(self):
        return {'host':'mqtt.example.com','port':8884,'deviceId':'11111111-1111-4111-8111-111111111111',
                'name':'Avi "map"','password':'synthetic-private-key-123456789'}

    def test_private_source_and_nonsecret_manifest(self):
        settings=self.settings()
        with tempfile.TemporaryDirectory() as d:
            out=Path(d)/'private'
            source=private.prepare(out,settings,'00007cd036858428',archive(),'commit123')
            header=(source/'include/private_monitor_enrollment.h').read_text()
            self.assertIn(settings['password'],header)
            self.assertIn('0x00007cd036858428ULL',header)
            self.assertIn('Avi \\"map\\"',header)
            self.assertIn('s.enabled=1',header)
            self.assertNotIn(settings['password'],(out/'bundle.json').read_text())
            self.assertNotIn(settings['password'],(out/'READ_ME_PRIVATE.txt').read_text())
            self.assertFalse(json.loads((out/'bundle.json').read_text())['public_release_allowed'])
            with self.assertRaises(FileExistsError):private.prepare(out,settings,'00007cd036858428',archive(),'commit123')

    def test_bad_unit_and_key(self):
        for unit in ('','COM5','../../bad','0000000000000000','00007CD036858428'):
            with self.assertRaises(ValueError):private.private_header(self.settings(),unit)
        settings=self.settings();settings['password']='short'
        with self.assertRaises(ValueError):private.private_header(settings,'00007cd036858428')

    def test_source_archive_cannot_supply_credentials_or_escape(self):
        for name in ('include/secrets.h','include/private_monitor_enrollment.h','../escape','/absolute','C:/drive-escape'):
            with tempfile.TemporaryDirectory() as d:
                with self.assertRaises(ValueError):private.extract_source(archive(name),Path(d))

if __name__=='__main__':unittest.main()
