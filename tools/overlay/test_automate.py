# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
import io,json,sys,tempfile,threading,unittest,zipfile,http.client
from pathlib import Path
from unittest import mock
sys.path.insert(0,str(Path(__file__).resolve().parent))
import automate as a
import overlay as o

def zipped(copper=b'%MOMM*%\nX100Y200D01*',color=b'blank'):
 b=io.BytesIO()
 with zipfile.ZipFile(b,'w') as z:
  z.writestr('board.GTL',copper);z.writestr('map.FCTS',color)
 return b.getvalue()

class WorkflowTests(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
  self.blank=self.root/'blank.zip';self.blank.write_bytes(zipped())
  prepared=self.root/'prepared';prepared.mkdir();image=b'test image bytes';(prepared/'prepared-map.png').write_bytes(image)
  (prepared/'manifest.json').write_text(json.dumps(dict(artworkSha256=o.digest(image),placement=dict(widthMm=342.31,heightMm=297.81,startXmm=0,startYmm=297.81))))
  self.output=self.root/'output';self.output.mkdir();self.job=a.Job(self.blank,prepared,self.output)
 def tearDown(self):self.tmp.cleanup()
 def test_baseline_then_finish_preserves_base(self):
  self.assertRaises(ValueError,self.job.finish,zipped(color=b'map'))
  self.job.baseline_check(zipped());self.job.finish(zipped(color=b'map'))
  result=o.archive(self.output/'result/Gerbers-with-prepared-artwork.zip')
  self.assertEqual(result['board.GTL'],o.archive(self.blank)['board.GTL']);self.assertEqual(result['map.FCTS'],b'map')
  self.assertEqual(self.job.state,'complete');self.assertRaises(ValueError,self.job.baseline_check,zipped())
 def test_wrong_template_and_changed_circuit_rejected(self):
  self.assertRaises(ValueError,self.job.baseline_check,zipped(copper=b'changed'))
  self.assertEqual(self.job.state,'waiting');self.assertFalse((self.output/'result').exists())
 def test_changed_circuit_in_map_rejected(self):
  self.job.baseline_check(zipped());self.assertRaises(ValueError,self.job.finish,zipped(copper=b'changed',color=b'map'))
  self.assertFalse((self.output/'result').exists())
 def test_loopback_token_and_http_pipeline(self):
  server=a.HTTPServer(('127.0.0.1',0),a.handler_for(self.job));thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
  def call(path,body=None):
   c=http.client.HTTPConnection('127.0.0.1',server.server_port,timeout=3);c.request('POST' if body else 'GET',path,body=body);r=c.getresponse();value=json.loads(r.read());code=r.status;c.close();return code,value
  try:
   self.assertEqual(call('/wrong/job')[0],403);self.assertEqual(self.job.state,'waiting')
   prefix='/'+self.job.token+'/';self.assertEqual(call(prefix+'job')[1]['state'],'waiting')
   self.assertEqual(call(prefix+'baseline',zipped())[0],200)
   self.assertEqual(call(prefix+'finish',zipped(color=b'map'))[1]['state'],'complete')
  finally:server.shutdown();thread.join();server.server_close()
 def test_extension_package(self):
  out=self.root/'test.eext';a.extension_package(out)
  with zipfile.ZipFile(out) as z:
   self.assertEqual(set(z.namelist()),{'extension.json','dist/index.js'})
   self.assertIn(b'edaEsbuildExportName',z.read('dist/index.js'))
 def test_old_gui_version_never_invoked_as_cli(self):
  if sys.platform!='win32':self.skipTest('Windows preflight')
  with mock.patch.object(a.subprocess,'run',return_value=mock.Mock(stdout='3.2.149')) as run, mock.patch.object(a,'cli_call') as cli:
   self.assertRaises(ValueError,a.run_cli,'old.exe',self.blank,'http://127.0.0.1:1/'+'a'*64);cli.assert_not_called();self.assertEqual(run.call_count,1)
 def test_cli_closes_only_its_own_session_on_failure(self):
  with mock.patch.object(a.subprocess,'run',return_value=mock.Mock(stdout='4.1.60')):
   with mock.patch.object(a,'cli_call',side_effect=[{},dict(sessionId='created-by-this-job'),ValueError('export failed'),{}]) as cli:
    self.assertRaises(ValueError,a.run_cli,'new.exe',self.blank,'http://127.0.0.1:1/'+'a'*64)
    self.assertEqual(cli.call_args.args,('new.exe','session','close','--session','created-by-this-job','--destroy'))

if __name__=='__main__':unittest.main()
