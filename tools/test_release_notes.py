# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
import copy,json,tempfile,unittest
from pathlib import Path
import release_notes as rn
import update_release_pages as sync
class Tests(unittest.TestCase):
 def test_full_historical_notes(self):
  self.assertEqual(rn.page_body({'tag_name':'v0.2.98'},'# Notes\n\n- A\n- B'), '# Notes\n\n- A\n- B\n')
 def test_latest_inline_and_online_detail(self):
  r={'tag_name':'v0.2.99','body':'old','padding':'x'*7700}
  text=rn.page_body(r,'full notes',True,'### Changes\n- Steady **! Alert**')
  self.assertIn('### Changes',text);self.assertIn('**! Alert**',text);self.assertIn('/blob/main/docs/RELEASE_0.2.99.md',text);self.assertNotIn('/download/',text)
  self.assertLessEqual(rn.metadata_bytes(r,text),rn.METADATA_LIMIT)
  with self.assertRaises(ValueError):rn.page_body(r,'notes',True,'x'*1000)
 def test_required_and_legacy_text(self):
  with self.assertRaises(ValueError):rn.page_body({},'')
  with self.assertRaises(ValueError):rn.page_body({'tag_name':'v0.2.99'},'notes',True)
  with tempfile.TemporaryDirectory() as tmp:
   p=Path(tmp)/'legacy';p.write_bytes(b'Old notes \x96 readable');self.assertEqual(rn.read_notes(p),'Old notes \u2013 readable')
 def test_index_sorted_and_drafts_excluded(self):
  r=[{'tag_name':'v0.2.9','html_url':'nine'},{'tag_name':'v0.2.100','html_url':'hundred'},{'tag_name':'v0.2.101','html_url':'draft','draft':True}]
  text=rn.changelog(r);self.assertLess(text.index('0.2.100'),text.index('0.2.9'));self.assertNotIn('0.2.101',text)
 def test_sync_preserves_latest_assets_and_backup(self):
  releases=[{'id':1,'tag_name':'v0.2.98','body':'old','html_url':'older','assets':[{'id':11,'name':'firmware','size':123}]},{'id':2,'tag_name':'v0.2.99','body':'old','html_url':'latest','assets':[{'id':22,'name':'firmware','size':123}]},{'id':3,'tag_name':'v0.3.0','draft':True,'body':'private'}]
  calls=[]
  def api(path,method='GET',data=None):
   if method=='PATCH':
    calls.append((path,copy.deepcopy(data)));r=next(r for r in releases if str(r['id'])==path.rsplit('/',1)[1]);r['body']=data['body'];return copy.deepcopy(r)
   return copy.deepcopy(releases[1] if path.endswith('/latest') else releases)
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);(root/'docs').mkdir()
   for version in ['0.2.98','0.2.99']:(root/'docs'/('RELEASE_'+version+'.md')).write_text('# Notes\n\nFull details')
   self.assertEqual(sync.sync_pages(api,root,'Inline changes'),2);self.assertEqual(calls,[])
   backup=root/'backup.json';self.assertEqual(sync.sync_pages(api,root,'Inline changes',True,backup),2)
   self.assertEqual(calls[0][1]['make_latest'],'false');self.assertEqual(calls[1][1]['make_latest'],'true')
   self.assertEqual(json.loads(backup.read_text())[0]['body'],'old');self.assertEqual(releases[2]['body'],'private');self.assertTrue((root/'CHANGELOG.md').exists())
if __name__=='__main__':unittest.main()
