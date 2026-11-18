# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
import importlib.util, json, tempfile, unittest, zipfile, io, math
from pathlib import Path
from unittest import mock
try:
 import pymupdf
 HAVE_RENDERER=True
except ImportError:HAVE_RENDERER=False
from PIL import Image
spec=importlib.util.spec_from_file_location('overlay',Path(__file__).with_name('overlay.py'));o=importlib.util.module_from_spec(spec);spec.loader.exec_module(o)
OUTLINE=b'%FSLAX45Y45*%\n%MOMM*%\nG01X0Y0D02*\nG01X0Y8000000D01*\nG01X10000000Y8000000D01*\nG01X10000000Y0D01*\nG01X0Y0D01*\nM02*'
def zipwrite(path,files):
 with zipfile.ZipFile(path,'w') as z:
  for name,data in files.items():z.writestr(name,data)
def workbook(path,points):
 ns='http://schemas.openxmlformats.org/spreadsheetml/2006/main';rows=[]
 for n,row in enumerate([['Designator','Mid X','Mid Y'],*[[name,str(x)+'mm',str(y)+'mm'] for name,(x,y) in points.items()]],1):
  cells=''.join(f'<c r="{chr(65+i)}{n}" t="inlineStr"><is><t>{v}</t></is></c>' for i,v in enumerate(row));rows.append('<row>'+cells+'</row>')
 zipwrite(path,{'xl/worksheets/sheet1.xml':f'<worksheet xmlns="{ns}"><sheetData>'+''.join(rows)+'</sheetData></worksheet>'})
class Tests(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.p=Path(self.tmp.name)
  self.a=self.p/'baseline.zip';self.b=self.p/'candidate.zip';self.files={'outline.GKO':OUTLINE,'copper.GTL':b'G04 generated date*\n%MOMM*%\nX10Y20D01*','mask.GTS':b'mask','holes.DRL':b'M48\n;date\nT01C0.2\n%\nX1Y2','color.FCTS':b'opaque'};zipwrite(self.a,self.files)
  self.source=self.p/'source.png';Image.new('RGBA',(100,80),'red').save(self.source)
  self.cpl=self.p/'placements.xlsx';self.points={f'LED{i+1}':p for i,p in enumerate([(5,75),(95,75),(5,5),(95,5),(50,40),(50,75),(75,20)])};workbook(self.cpl,self.points)
  self.landmarks=[{'led':n,'sourcePx':[x,80-y]} for n,(x,y) in self.points.items()];self.lm=self.p/'points.json';self.lm.write_text(json.dumps(self.landmarks))
 def tearDown(self):self.tmp.cleanup()
 def test_outline_and_placements(self):
  self.assertEqual(o.board(self.a)['boundsMm'],[0,0,100,80]);self.assertEqual(o.placements(self.cpl),{n:list(v) for n,v in self.points.items()})
 def test_protected_layers(self):
  zipwrite(self.b,self.files);self.assertTrue(o.compare(self.a,self.b)['nonArtworkIntegrityPassed'])
  changed={**self.files,'color.FCTS':b'new color'};zipwrite(self.b,changed);r=o.compare(self.a,self.b);self.assertTrue(r['nonArtworkIntegrityPassed']);self.assertFalse(r['artworkAlignmentVerified'])
  changed['copper.GTL']+=b'X2Y3D01*';zipwrite(self.b,changed);self.assertFalse(o.compare(self.a,self.b)['nonArtworkIntegrityPassed'])
 def test_comments_not_geometry(self):
  changed={**self.files,'copper.GTL':self.files['copper.GTL'].replace(b'generated date',b'another date')};zipwrite(self.b,changed);r=o.compare(self.a,self.b);self.assertTrue(r['nonArtworkIntegrityPassed']);self.assertIn('comments_only',[v['status'] for v in r['members']])
  changed['mask.GTS']=b'mask changed';zipwrite(self.b,changed);self.assertFalse(o.compare(self.a,self.b)['nonArtworkIntegrityPassed'])
 def test_removed_unknown_member(self):
  changed=dict(self.files);del changed['color.FCTS'];zipwrite(self.b,changed);self.assertFalse(o.compare(self.a,self.b)['nonArtworkIntegrityPassed'])
  changed=dict(self.files);changed['extra.txt']=b'new';zipwrite(self.b,changed);self.assertFalse(o.compare(self.a,self.b)['nonArtworkIntegrityPassed'])
 def test_zip_path_and_duplicates(self):
  zipwrite(self.b,{'../evil.GTL':b'x'});self.assertRaises(ValueError,o.archive,self.b)
  zipwrite(self.b,{'A.GTL':b'x','a.gtl':b'y'});self.assertRaises(ValueError,o.archive,self.b)
 def test_fit_and_outputs(self):
  p=o.fit_profile(self.source,self.lm,self.a,self.cpl,.1);self.assertLess(p['maxAnchorResidualMm'],1e-10);profile=self.p/'profile.json';o.save_json(profile,p)
  out=self.p/'prepared';o.prepare(self.source,profile,self.a,self.cpl,out,72)
  manifest=o.load_json(out/'manifest.json');self.assertFalse(manifest['manufacturingApproval']);self.assertEqual(manifest['widthMm'],100)
  image=o.image_file(out/'prepared-artwork.png');self.assertEqual(image.getpixel((100,100)),(255,0,0,255))
  self.assertRaises(ValueError,o.prepare,self.source,profile,self.a,self.cpl,out,72)
  source_hash=o.digest(o.read(self.source));gerber_hash=o.digest(o.read(self.a));self.assertEqual(source_hash,p['sourceSha256']);self.assertEqual(gerber_hash,p['gerberSha256'])
 def test_wrong_source_rejected(self):
  p=o.fit_profile(self.source,self.lm,self.a,self.cpl,.1);profile=self.p/'profile.json';o.save_json(profile,p);Image.new('RGB',(100,80),'blue').save(self.source)
  self.assertRaises(ValueError,o.prepare,self.source,profile,self.a,self.cpl,self.p/'bad',72)
 def test_bad_anchors(self):
  self.lm.write_text(json.dumps(self.landmarks[:3]));self.assertRaises(ValueError,o.fit_profile,self.source,self.lm,self.a,self.cpl,.1)
  self.landmarks[-1]['sourcePx']=[50,50];self.lm.write_text(json.dumps(self.landmarks));self.assertRaises(ValueError,o.fit_profile,self.source,self.lm,self.a,self.cpl,.1)
 def test_profile_tampering_and_mirroring(self):
  p=o.fit_profile(self.source,self.lm,self.a,self.cpl,.1);p['sourcePxToGerberMm'][0][2]+=2;profile=self.p/'tampered.json';o.save_json(profile,p)
  self.assertRaises(ValueError,o.prepare,self.source,profile,self.a,self.cpl,self.p/'bad',72)
  for point in self.landmarks:point['sourcePx'][0]=100-point['sourcePx'][0]
  self.lm.write_text(json.dumps(self.landmarks));self.assertRaises(ValueError,o.fit_profile,self.source,self.lm,self.a,self.cpl,.1)
 def test_production_embedded_copy(self):
  production=self.p/'production.zip';zipwrite(production,{'original.zip':self.a.read_bytes(),'factory/top':b'CAM'})
  report=o.production_audit(production,self.a);self.assertTrue(report['containsIdenticalSubmittedContents']);self.assertFalse(report['camGeometryEquivalenceVerified'])
  zipwrite(production,{'original.zip':b'bad'});self.assertRaises(zipfile.BadZipFile,o.production_audit,production,self.a)
 def test_crop_pin_and_bounds(self):
  out=self.p/'crop.png';sha=o.digest(o.read(self.source));o.crop(self.source,out,[1,2,90,70],sha)
  self.assertEqual(o.image_file(out).size,(89,68));self.assertEqual(o.load_json(str(out)+'.source.json')['sourceSha256'],sha)
  self.assertRaises(ValueError,o.crop,self.source,self.p/'bad.png',[-1,0,100,80],sha)
  self.assertRaises(ValueError,o.crop,self.source,self.p/'bad.png',[0,0,100,80],'0'*64)
 def test_notch_and_mask(self):
  geometry=o.board(self.a);self.assertEqual(geometry['outlineMm'][0],geometry['outlineMm'][-1])
  altered=OUTLINE.replace(b'%MOMM*%',b'%MOIN*%');zipwrite(self.b,{'outline.GKO':altered});self.assertRaises(ValueError,o.board,self.b)
 def test_package_preserves_exact_non_artwork_bytes(self):
  changed={**self.files,'color.FCTS':b'new artwork','copper.GTL':self.files['copper.GTL'].replace(b'generated date',b'new export comment')};zipwrite(self.b,changed)
  out=self.p/'package';o.package(self.a,self.b,out);contents=o.archive(out/'Gerbers-with-prepared-artwork.zip')
  self.assertEqual(contents['copper.GTL'],self.files['copper.GTL']);self.assertEqual(contents['color.FCTS'],b'new artwork')
  self.assertTrue(o.load_json(out/'integrity-report.json')['protectedPayloadsByteIdentical'])
  changed['holes.DRL']+=b'X100Y100';zipwrite(self.b,changed)
  self.assertRaises(ValueError,o.package,self.a,self.b,self.p/'bad-package');self.assertFalse((self.p/'bad-package').exists())
 def test_fetch_pins_source_and_redirect_host(self):
  destination=self.p/'download';body=b'known edition'
  with mock.patch.object(o.urllib.request,'build_opener') as build:
   build.return_value.open.return_value.__enter__.return_value.read.return_value=body
   o.fetch('https://www.mta.info/map/123',o.digest(body),destination);self.assertEqual(destination.read_bytes(),body)
   self.assertRaises(ValueError,o.fetch,'https://www.mta.info/map/123','0'*64,self.p/'wrong');self.assertFalse((self.p/'wrong').exists())
   handler=build.call_args.args[0]();self.assertRaises(ValueError,handler.redirect_request,None,None,302,'',{},'https://example.com/map')
  self.assertRaises(ValueError,o.fetch,'http://www.mta.info/map/123',o.digest(body),self.p/'http')
  self.assertRaises(ValueError,o.fetch,'https://mta.info.example.com/map',o.digest(body),self.p/'spoof')
 @unittest.skipUnless(HAVE_RENDERER,'Install PyMuPDF to test PDF/SVG rasterization')
 def test_svg_render_and_external_resources(self):
  source=self.p/'map.svg';source.write_text('<svg xmlns="http://www.w3.org/2000/svg" width="100" height="80"><rect width="100" height="80" fill="red"/></svg>')
  output=self.p/'render.png';o.rasterize(source,output,72,1,o.digest(o.read(source)));self.assertTrue(output.exists());self.assertIn('renderer',o.load_json(str(output)+'.source.json'))
  source.write_text('<svg xmlns="http://www.w3.org/2000/svg"><image href="https://example.com/private"/></svg>')
  self.assertRaises(ValueError,o.rasterize,source,self.p/'external.png',72,1,o.digest(o.read(source)))
 def test_no_overwrite(self):
  path=self.p/'existing';path.write_bytes(b'keep');self.assertRaises(FileExistsError,o.write_new,path,b'new');self.assertEqual(path.read_bytes(),b'keep')
 def test_picker_offline(self):
  path=self.p/'picker.html';o.pick(self.source,self.a,self.cpl,path);s=path.read_text();self.assertIn('data:image/png;base64',s);self.assertNotIn('<!--SOURCE_',s);self.assertNotIn('fetch(',s)
if __name__=='__main__':unittest.main()
