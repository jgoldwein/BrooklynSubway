#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Prepare registered PCB artwork; never patch EasyEDA's opaque color format."""
from __future__ import annotations
import argparse, base64, hashlib, io, json, math, re, sys
import urllib.parse, urllib.request, zipfile
from pathlib import Path, PurePosixPath
import xml.etree.ElementTree as ET

MAX_FILE = 128 * 1024 * 1024
MAX_ARCHIVE = 256 * 1024 * 1024
COLOR = {'.FCTS', '.FCBS', '.FCBO', '.FCBM'}
GERBER = {'.GTL', '.GBL', '.GTO', '.GBO', '.GTS', '.GBS', '.GTP', '.GBP', '.GKO', '.GDL', '.GDD'}

def digest(data): return hashlib.sha256(data).hexdigest()
def read(path):
    path = Path(path)
    if path.stat().st_size > MAX_FILE: raise ValueError('Input exceeds 128 MiB')
    return path.read_bytes()
def write_new(path, data):
    path = Path(path); path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('xb') as f: f.write(data)
def save_json(path, data): write_new(path, (json.dumps(data, indent=2, allow_nan=False)+'\n').encode())
def load_json(path): return json.loads(read(path).decode('utf-8-sig'))
def check_hash(data, expected):
    if not re.fullmatch('[0-9a-fA-F]{64}', expected or '') or digest(data) != expected.lower():
        raise ValueError('SHA-256 mismatch; do not substitute a different map edition or board archive')
def archive_bytes(data):
    result = {}; total = 0; names = set()
    with zipfile.ZipFile(io.BytesIO(data)) as z:
        if len(z.infolist()) > 256: raise ValueError('Too many ZIP members')
        for i in z.infolist():
            if i.is_dir(): continue
            if i.flag_bits & 1:raise ValueError('Encrypted ZIP members are unsupported')
            name = i.filename; p = PurePosixPath(name)
            if p.is_absolute() or '..' in p.parts or '\\' in name or ':' in name or name.casefold() in names:
                raise ValueError('Unsafe or duplicate ZIP member')
            names.add(name.casefold()); total += i.file_size
            if i.file_size > MAX_FILE or total > MAX_ARCHIVE: raise ValueError('ZIP is too large')
            result[name] = z.read(i)
    if not result: raise ValueError('Empty archive')
    return result

def archive(path):return archive_bytes(read(path))

def normalized(name, data):
    """Ignore only non-executable Gerber/Excellon comments and line endings."""
    ext = Path(name).suffix.upper()
    if ext not in GERBER | {'.DRL'}: return data
    try: text = data.decode('ascii')
    except UnicodeDecodeError: return data
    if ext == '.DRL':
        text = '\n'.join(line for line in text.splitlines() if not line.lstrip().startswith(';'))
    else:
        text = re.sub(r'(?m)^\s*G04[^*]*\*', '', text)
        text = re.sub(r'%TF\.CreationDate,[^*]*\*%', '', text)
    return '\n'.join(line.strip() for line in text.splitlines() if line.strip()).encode()

def gerber_aperture_commands(data):
    """Narrow equivalence for unchanged commands with renamed/unused apertures.

    All non-definition commands and macro bodies remain exact. Unsupported
    aperture blocks, redefinitions or non-standalone selections fail closed.
    """
    try:text=data.decode('ascii')
    except UnicodeDecodeError:return None
    if '%AB' in text:return None
    definitions={};invalid=False
    def definition(match):
        nonlocal invalid
        key=int(match[1])
        if key<10 or key in definitions:invalid=True
        definitions[key]=match[2]
        return ''
    text=re.sub(r'%ADD(\d+)([^*%]+)\*%',definition,text)
    if invalid or '%AD' in text:return None
    # Only match selection statements outside parameter/macro blocks.
    parts=re.split(r'(%[^%]*%)',text)
    result=[]
    for part in parts:
        if part.startswith('%'):
            result.append(part);continue
        cursor=0
        for match in re.finditer(r'(?:G54)?D([1-9]\d+)\*',part):
            key=int(match[1])
            if key<10:continue
            prefix=part[cursor:match.start()]
            if re.search(r'D0*[1-9]\d+\*',prefix) or (match.start() and part[match.start()-1] not in '*\n\r'):
                return None
            if key not in definitions:return None
            result.append(prefix);result.append(('aperture',definitions[key]));cursor=match.end()
        tail=part[cursor:]
        if re.search(r'D0*[1-9]\d+\*',tail):return None
        result.append(tail)
    # Definitions were removed; their adjacent empty lines carry no commands.
    return tuple(('text','\n'.join(v for v in item.splitlines() if v)) if isinstance(item,str) else item for item in result if item)


def same_aperture_commands(name, old, new):
    if Path(name).suffix.upper() not in GERBER:return False
    a=gerber_aperture_commands(normalized(name,old));b=gerber_aperture_commands(normalized(name,new))
    return a is not None and b is not None and a==b


def compare(baseline, candidate):
    a, b = archive(baseline), archive(candidate); items = []; failed = []
    for name in sorted(a.keys() | b.keys()):
        old, new = a.get(name), b.get(name); color = Path(name).suffix.upper() in COLOR
        if old is None or new is None: status = 'member_added_or_removed'; failed.append(name)
        elif old == new: status = 'identical'
        elif color: status = 'color_artwork_changed_requires_visual_review'
        elif normalized(name, old) == normalized(name, new): status = 'comments_only'
        elif same_aperture_commands(name,old,new):status = 'aperture_renumbering_or_unused_definitions_only'
        else: status = 'protected_content_changed'; failed.append(name)
        items.append(dict(name=name, status=status, baselineSha256=digest(old) if old is not None else None,
                          candidateSha256=digest(new) if new is not None else None))
    return dict(schema=1, baselineSha256=digest(read(baseline)), candidateSha256=digest(read(candidate)),
                nonArtworkIntegrityPassed=not failed, failedMembers=failed, members=items,
                artworkAlignmentVerified=False, manufacturingApproval=False,
                note='Color artwork is opaque. This report does not approve alignment, pad clearance, licensing or manufacture.')

def package(baseline, candidate, output):
    report=compare(baseline,candidate)
    if not report['nonArtworkIntegrityPassed']:raise ValueError('Protected fabrication content changed; package refused')
    original,new=archive(baseline),archive(candidate);out=Path(output)
    if out.exists():raise ValueError('Output directory already exists; choose a new path')
    buffer=io.BytesIO()
    with zipfile.ZipFile(buffer,'w',compression=zipfile.ZIP_DEFLATED) as z:
        for name,data in original.items():z.writestr(name,new[name] if Path(name).suffix.upper() in COLOR else data)
    result=buffer.getvalue();contents=archive_bytes(result)
    if any(contents[n]!=data for n,data in original.items() if Path(n).suffix.upper() not in COLOR):raise ValueError('Protected payload verification failed')
    out.mkdir(parents=True)
    write_new(out/'Gerbers-with-prepared-artwork.zip',result)
    report['packagedArchiveSha256']=digest(result);report['protectedPayloadsByteIdentical']=True
    save_json(out/'integrity-report.json',report)

def production_audit(production, submitted):
    originals=archive(submitted); expected={n:digest(d) for n,d in originals.items()}; matches=[]
    for name,data in archive(production).items():
        if Path(name).suffix.lower()!='.zip':continue
        embedded=archive_bytes(data)
        hashes={n:digest(d) for n,d in embedded.items()}
        matches.append(dict(member=name,embeddedArchiveSha256=digest(data),matchesSubmittedContents=hashes==expected,
                            members=len(hashes),differentMembers=sorted(n for n in hashes.keys()|expected.keys() if hashes.get(n)!=expected.get(n))))
    return dict(schema=1,productionArchiveSha256=digest(read(production)),submittedArchiveSha256=digest(read(submitted)),
                containsIdenticalSubmittedContents=any(m['matchesSubmittedContents'] for m in matches),embeddedArchives=matches,
                camGeometryEquivalenceVerified=False,exactArtworkRegistrationRecovered=False,
                note='An embedded original identifies the submitted source. It does not equate manufacturer CAM layers to design layers.')

def board(path):
    files = archive(path); candidates = [data for name, data in files.items() if Path(name).suffix.upper()=='.GKO']
    if len(candidates)!=1: raise ValueError('Expected one board-outline GKO')
    text = candidates[0].decode('ascii')
    # Deliberately narrow: fail on unfamiliar geometry rather than approximate it.
    if '%MOMM*%' not in text or '%FSLAX45Y45*%' not in text or re.search(r'G0[23][^0-9]',text):
        raise ValueError('Unsupported outline format; requires review of units/format/arcs')
    commands = re.findall(r'(?:G01)?X(-?\d+)Y(-?\d+)D0([12])\*',text)
    points=[]
    for x,y,d in commands:
        if d=='2' and points: raise ValueError('Multiple outline paths require manual review')
        points.append([int(x)/100000,int(y)/100000])
    if len(points)<4 or points[0]!=points[-1]: raise ValueError('Outline must be one closed linear path')
    xs,ys=zip(*points); return dict(outlineMm=points, boundsMm=[min(xs),min(ys),max(xs),max(ys)])

def placements(path):
    ns={'m':'http://schemas.openxmlformats.org/spreadsheetml/2006/main'}
    with zipfile.ZipFile(io.BytesIO(read(path))) as z:
        total=sum(i.file_size for i in z.infolist())
        if total>MAX_ARCHIVE or any(i.file_size>MAX_FILE for i in z.infolist()): raise ValueError('Workbook too large')
        strings=[]
        if 'xl/sharedStrings.xml' in z.namelist():
            root=ET.fromstring(z.read('xl/sharedStrings.xml'))
            strings=[''.join(n.itertext()) for n in root.findall('m:si',ns)]
        root=ET.fromstring(z.read('xl/worksheets/sheet1.xml')); rows=[]
        for row in root.findall('.//m:row',ns):
            values=[]
            for cell in row.findall('m:c',ns):
                col=re.match('[A-Z]+',cell.attrib['r']).group(); typ=cell.get('t'); node=cell.find('m:v',ns)
                value=node.text if node is not None else ''.join(cell.find('m:is',ns).itertext()) if cell.find('m:is',ns) is not None else ''
                if typ=='s': value=strings[int(value)]
                values.append((col,value))
            rows.append(dict(values))
    if not rows: raise ValueError('Empty placement workbook')
    headers={re.sub(r'[^a-z0-9]','',str(v).lower()):k for k,v in rows[0].items()}
    def column(*labels):
        for label in labels:
            if label in headers:return headers[label]
        raise ValueError('Unsupported placement columns: '+', '.join(headers))
    ref=column('designator','reference'); x=column('midx','centerx'); y=column('midy','centery')
    result={}
    for row in rows[1:]:
        name=row.get(ref,'')
        if not re.fullmatch(r'LED\d+',name):continue
        if name in result:raise ValueError('Duplicate LED designator')
        result[name]=[float(row[x].removesuffix('mm')),float(row[y].removesuffix('mm'))]
        if not all(math.isfinite(v) for v in result[name]):raise ValueError('Non-finite placement')
    if not result:raise ValueError('No LED placements found')
    return result

def image_file(path):
    from PIL import Image
    Image.MAX_IMAGE_PIXELS=80_000_000
    im=Image.open(io.BytesIO(read(path)))
    if im.width*im.height>80_000_000: raise ValueError('Raster too large')
    if getattr(im,'n_frames',1)!=1: raise ValueError('Use one image frame')
    result=im.convert('RGBA');result.info.clear();return result

def fit_profile(source, landmarks, gerber, cpl, tolerance):
    import numpy as np
    im=image_file(source); points=load_json(landmarks); leds=placements(cpl)
    if not isinstance(points,list) or len(points)<6: raise ValueError('Use at least six distinct anchors, spread around the map')
    xy=[]; target=[]; seen=set()
    for p in points:
        name=p['led']; x,y=map(float,p['sourcePx'])
        if name in seen or name not in leds or not(0<=x<im.width and 0<=y<im.height):raise ValueError('Invalid, repeated or out-of-image anchor')
        seen.add(name);xy.append([x,y,1]);target.append(leds[name])
    a=np.array(xy);b=np.array(target); m,_,rank,_=np.linalg.lstsq(a,b,rcond=None)
    if rank!=3:raise ValueError('Collinear anchors cannot define an affine transform')
    error=np.linalg.norm(a@m-b,axis=1); maximum=float(error.max())
    if not math.isfinite(tolerance) or tolerance<=0 or tolerance>1:raise ValueError('Tolerance must be in (0, 1] mm')
    if maximum>tolerance:raise ValueError(f'Anchor residual {maximum:.4f} mm exceeds {tolerance} mm; inspect crop/source/layout')
    # Prevent a tiny cluster from appearing to verify a full-board calibration.
    spread=np.ptp(a[:,:2],axis=0)/np.array(im.size)
    if min(spread)<0.45:raise ValueError('Anchors must span at least 45% of image width and height')
    det=float(np.linalg.det(m[:2,:].T))
    if abs(det)<1e-12:raise ValueError('Singular placement')
    if det>0:raise ValueError('Mirrored mapping rejected; source pixels have Y down and Gerber coordinates have Y up')
    return dict(schema=1, status='DRAFT_REQUIRES_INDEPENDENT_REVIEW', sourceSha256=digest(read(source)),
                sourcePixels=list(im.size), gerberSha256=digest(read(gerber)), placementSha256=digest(read(cpl)),
                board=board(gerber), sourcePxToGerberMm=m.T.tolist(), anchors=points,
                maxAnchorResidualMm=maximum, toleranceMm=tolerance,
                exactSubmittedArtworkVerified=False, manufacturingApproval=False,
                note='Anchor fit is not independent proof of artwork identity; verify additional stations and JLCPCB preview.')

def prepare(source, profile, gerber, cpl, out, dpi):
    import numpy as np
    from PIL import Image, ImageDraw
    if not 72<=dpi<=600:raise ValueError('DPI must be 72..600')
    p=load_json(profile)
    if p.get('schema')!=1:raise ValueError('Unsupported profile schema')
    for file,key in [(source,'sourceSha256'),(gerber,'gerberSha256'),(cpl,'placementSha256')]: check_hash(read(file),p[key])
    im=image_file(source)
    if list(im.size)!=p['sourcePixels']:raise ValueError('Source dimensions changed')
    geometry=board(gerber)
    if geometry!=p['board']:raise ValueError('Board geometry mismatch')
    matrix=np.array(p['sourcePxToGerberMm'],dtype=float)
    if matrix.shape!=(2,3) or not np.isfinite(matrix).all():raise ValueError('Invalid affine matrix')
    transform=np.vstack([matrix,[0,0,1]])
    if float(np.linalg.det(transform))>=-1e-12:raise ValueError('Singular or mirrored transform')
    anchors=p.get('anchors',[]);leds=placements(cpl);seen=set();errors=[];source_points=[]
    if len(anchors)<6:raise ValueError('Profile requires at least six anchors')
    for anchor in anchors:
        name=anchor['led'];x,y=map(float,anchor['sourcePx'])
        if name in seen or name not in leds or not(0<=x<im.width and 0<=y<im.height):raise ValueError('Invalid profile anchor')
        seen.add(name);source_points.append([x,y]);errors.append(float(np.linalg.norm(matrix@np.array([x,y,1])-leds[name])))
    tolerance=float(p['toleranceMm'])
    if not math.isfinite(tolerance) or not 0<tolerance<=1 or max(errors)>tolerance:raise ValueError('Profile matrix no longer fits its anchors')
    if min(np.ptp(source_points,axis=0)/np.array(im.size))<.45:raise ValueError('Profile anchors are too clustered')
    x0,y0,x1,y1=geometry['boundsMm']; scale=dpi/25.4
    width,height=round((x1-x0)*scale),round((y1-y0)*scale)
    if width*height>80_000_000:raise ValueError('Output raster too large')
    # Exact requested mechanical dimensions; actual pixel scale accounts for rounding.
    sx,sy=width/(x1-x0),height/(y1-y0)
    output_to_mm=np.array([[1/sx,0,x0],[0,-1/sy,y1],[0,0,1]])
    inverse=np.linalg.inv(transform)@output_to_mm
    art=im.transform((width,height),Image.Transform.AFFINE,tuple(inverse[:2].flat),resample=Image.Resampling.BICUBIC)
    def pixel(point):return ((point[0]-x0)*sx,(y1-point[1])*sy)
    mask=Image.new('L',art.size,0);ImageDraw.Draw(mask).polygon([pixel(v) for v in geometry['outlineMm']],fill=255)
    alpha=np.minimum(np.asarray(art.getchannel('A')),np.asarray(mask));art.putalpha(Image.fromarray(alpha))
    out=Path(out)
    if out.exists():raise ValueError('Output directory already exists; choose a new path')
    out.mkdir(parents=True)
    def png(image):
        b=io.BytesIO();image.save(b,format='PNG',dpi=(dpi,dpi));return b.getvalue()
    art_bytes=png(art);write_new(out/'prepared-artwork.png',art_bytes)
    review=Image.new('RGBA',art.size,'white');review.alpha_composite(art);draw=ImageDraw.Draw(review)
    draw.line([pixel(v) for v in geometry['outlineMm']],fill='black',width=max(1,round(sx*.12)))
    radius=max(2,round(sx*.7))
    for name,point in placements(cpl).items():
        x,y=pixel(point);draw.ellipse((x-radius,y-radius,x+radius,y+radius),outline='#ff00aa',width=max(1,round(sx*.15)))
        draw.text((x+radius,y),name,fill='#660066')
    review.thumbnail((1800,1800));write_new(out/'alignment-review.png',png(review))
    # SVG is a placement reference, not a substitute for EasyEDA's color fabrication export.
    svg=f'<svg xmlns="http://www.w3.org/2000/svg" width="{x1-x0:.6f}mm" height="{y1-y0:.6f}mm" viewBox="0 0 {width} {height}"><image width="{width}" height="{height}" href="data:image/png;base64,{base64.b64encode(art_bytes).decode()}"/></svg>'
    write_new(out/'placement-reference.svg',svg.encode())
    instructions=f'''# DRAFT artwork preparation - not an order-ready Gerber

Physical image size: {x1-x0:.6f} x {y1-y0:.6f} mm.
Gerber bounding rectangle: x {x0:.6f}..{x1:.6f}, y {y0:.6f}..{y1:.6f} mm.
PNG top-left corresponds to Gerber ({x0:.6f}, {y1:.6f}); Gerber Y increases upward.
Do not mirror. Preserve width AND height explicitly in EasyEDA; PNG DPI alone is not placement authority.
EasyEDA project origin may differ from the Gerber origin: verify that offset from known LED positions.

1. Compare alignment-review.png against independent stations around every edge and the center.
2. Open a COPY of the correct EasyEDA Pro project. Replace only the intended map image;
   retain legends, component markings, pad/drill clearances and other original artwork.
   This full-board-sized PNG does not automatically reproduce overlapping custom legends.
3. Import prepared-artwork.png on the intended color silkscreen layer. Use the dimensions
   and registration above, and verify against the LED locations. Do not infer alignment from size alone.
4. Export color fabrication files using EasyEDA Pro. Run the verify command against the baseline.
5. Review JLCPCB's full color production preview. This script does not approve manufacture or rights.

No source Gerber or PCB project was modified. Review the manifest for all source hashes.
'''
    write_new(out/'EASYEDA_IMPORT.md',instructions.encode())
    save_json(out/'manifest.json',dict(schema=1,profileSha256=digest(read(profile)),
        sourceSha256=p['sourceSha256'],baselineGerberSha256=p['gerberSha256'],placementSha256=p['placementSha256'],
        widthMm=x1-x0,heightMm=y1-y0,requestedDpi=dpi,actualPixels=[width,height],
        artworkSha256=digest(art_bytes),manufacturingApproval=False,profileStatus='DRAFT_REQUIRES_INDEPENDENT_REVIEW'))

def prepare_pdf(source, recipe_path, output, dpi=300):
    """Crop a hash-pinned PDF in page points and apply explicit import dimensions."""
    recipe=load_json(recipe_path)
    if recipe.get('schema')!=1:raise ValueError('Unsupported PDF crop recipe schema')
    spec=recipe.get('source',{});place=recipe.get('placement',{});box=recipe.get('cropBoxPt',[])
    def numbers(values,count):
        return isinstance(values,list) and len(values)==count and all(type(v) in (int,float) and math.isfinite(v) for v in values)
    if not numbers(box,4) or not(box[0]<box[2] and box[1]<box[3]):raise ValueError('Invalid PDF crop rectangle')
    size=spec.get('pageSizePt',[])
    if not numbers(size,2) or min(size)<=0:raise ValueError('Invalid recipe page dimensions')
    dims=[place.get('widthMm'),place.get('heightMm')]
    if not numbers(dims,2) or min(dims)<=0 or max(dims)>2000:raise ValueError('Invalid physical image dimensions')
    if not numbers([place.get('startXmm'),place.get('startYmm')],2):raise ValueError('Invalid image placement')
    if place.get('units')!='mm' or place.get('layer')!='Top Silkscreen Layer' or place.get('rotationDegrees')!=0 or place.get('mirror') is not False:
        raise ValueError('Recipe requires non-mirrored, unrotated top-silkscreen placement in mm')
    page_number=spec.get('page')
    if type(page_number)!=int or page_number<1:raise ValueError('Invalid PDF page number')
    if type(dpi)!=int or not 72<=dpi<=600:raise ValueError('DPI must be 72..600')
    width,height=[max(1,round(v/25.4*dpi)) for v in dims]
    if width*height>80_000_000 or (box[2]-box[0])*(box[3]-box[1])*(dpi/72)**2>80_000_000:raise ValueError('Raster too large')
    out=Path(output)
    if out.exists():raise ValueError('Output directory already exists; choose a new path')
    data=read(source);check_hash(data,spec.get('sha256'))
    import pymupdf
    from PIL import Image
    with pymupdf.open(stream=data,filetype='pdf') as doc:
        if doc.needs_pass or page_number>len(doc):raise ValueError('Invalid or password-protected source PDF')
        page=doc[page_number-1]
        if page.rotation or any(abs(a-b)>1e-5 for a,b in zip([page.rect.width,page.rect.height],size)):
            raise ValueError('PDF page geometry does not match the recipe')
        if not(0<=box[0]<box[2]<=page.rect.width and 0<=box[1]<box[3]<=page.rect.height):raise ValueError('Crop rectangle is outside the PDF page')
        pix=page.get_pixmap(matrix=pymupdf.Matrix(dpi/72,dpi/72),clip=pymupdf.Rect(box),alpha=False)
        image=Image.frombytes('RGB',(pix.width,pix.height),pix.samples)
        # Explicitly preserve the fit's independent horizontal and vertical scaling.
        extent=(box[0]*dpi/72-pix.x,box[1]*dpi/72-pix.y,box[2]*dpi/72-pix.x,box[3]*dpi/72-pix.y)
        image=image.resize((width,height),Image.Resampling.LANCZOS,box=extent)
        buffer=io.BytesIO();image.save(buffer,format='PNG',dpi=(dpi,dpi));art=buffer.getvalue()
    manifest=dict(schema=1,sourcePdfSha256=digest(data),recipeSha256=digest(read(recipe_path)),cropBoxPt=box,page=page_number,
        renderer='PyMuPDF '+pymupdf.VersionBind,actualPixels=[width,height],dpi=dpi,placement=place,
        scaling='Independent horizontal and vertical dimensions from the recorded user fit',artworkSha256=digest(art),
        profileStatus='DRAFT_REQUIRES_INDEPENDENT_REVIEW',manufacturingApproval=False)
    instruction=("# Import prepared map into the blank working copy\n\n"
        "Place > Image: choose prepared-map.png and keep original quality checked.\n\n"
        f"Layer: {place['layer']}\n\nWidth: {dims[0]:.3f} mm\n\nHeight: {dims[1]:.3f} mm\n\n"
        f"Start X: {place['startXmm']:.3f} mm\n\nStart Y: {place['startYmm']:.3f} mm\n\nRotation: 0 degrees\n\nMirror: No\n\n"
        "Unlock the aspect ratio if necessary; both physical dimensions are deliberate. PNG DPI alone is not an origin specification.\n\n"
        "Review widely separated station/LED locations and custom labels in the color preview. This is a draft crop/placement recipe, not manufacturing approval. "
        "Use the original manufacturing archive for electrical content; the inspected working project's circuitry differs.\n")
    out.mkdir(parents=True)
    write_new(out/'prepared-map.png',art)
    save_json(out/'manifest.json',manifest)
    save_json(out/'placement.json',place)
    write_new(out/'EASYEDA_IMPORT.md',instruction.encode())


def crop(source, output, box, sha):
    check_hash(read(source),sha);im=image_file(source)
    x0,y0,x1,y1=box
    if not(0<=x0<x1<=im.width and 0<=y0<y1<=im.height):raise ValueError('Crop rectangle is outside the source raster')
    result=im.crop(box);buffer=io.BytesIO();result.save(buffer,format='PNG');write_new(output,buffer.getvalue())
    save_json(str(output)+'.source.json',dict(sourceSha256=sha.lower(),cropBoxPx=list(box),rasterSha256=digest(read(output))))

def pick(source, gerber, cpl, output):
    im=image_file(source);buffer=io.BytesIO();im.save(buffer,format='PNG')
    info=dict(width=im.width,height=im.height,leds=placements(cpl),board=board(gerber))
    template=Path(__file__).with_name('landmarks.html').read_text(encoding='utf-8')
    page=template.replace('<!--SOURCE_DATA-->',json.dumps(info,allow_nan=False)).replace('<!--SOURCE_IMAGE-->',base64.b64encode(buffer.getvalue()).decode())
    write_new(output,page.encode())

def fetch(url, sha, out):
    def trusted(value):
        p=urllib.parse.urlsplit(value)
        return p.scheme=='https' and p.hostname and (p.hostname=='mta.info' or p.hostname.endswith('.mta.info')) and not p.username and not p.password and p.port in (None,443)
    class Redirect(urllib.request.HTTPRedirectHandler):
        def redirect_request(self,request,fp,code,msg,headers,newurl):
            if not trusted(newurl):raise ValueError('Redirect leaves official HTTPS MTA hosts')
            return super().redirect_request(request,fp,code,msg,headers,newurl)
    if not trusted(url):raise ValueError('Use an official https://...mta.info/... source URL')
    with urllib.request.build_opener(Redirect).open(url,timeout=30) as response:data=response.read(MAX_FILE+1)
    if len(data)>MAX_FILE:raise ValueError('Download exceeds limit')
    check_hash(data,sha);write_new(out,data)

def rasterize(source, output, dpi, page, sha):
    check_hash(read(source),sha)
    if not 72<=dpi<=600:raise ValueError('DPI must be 72..600')
    import pymupdf
    data=read(source)
    is_svg=Path(source).suffix.lower()=='.svg'
    if is_svg:
        root=ET.fromstring(data)
        if not root.tag.endswith('}svg'):raise ValueError('Invalid SVG root')
        for node in root.iter():
            if node.tag.rsplit('}',1)[-1] in {'script','foreignObject','style'}:raise ValueError('SVG active/style content is unsupported; use a flattened local SVG or PDF')
            for key,value in node.attrib.items():
                if key.rsplit('}',1)[-1]=='href' and not(value.startswith('#') or value.startswith('data:image/png;base64,') or value.startswith('data:image/jpeg;base64,') or value.startswith('data:application/vnd.iccprofile;base64,')):
                    raise ValueError('External SVG resources are unsupported')
                for url in re.findall(r'url\s*\(([^)]*)\)',value,re.I):
                    if not url.strip().strip("\"'").startswith('#'):raise ValueError('External SVG URL is unsupported')
    with pymupdf.open(stream=data,filetype='svg' if is_svg else 'pdf') as document:
        if document.needs_pass:raise ValueError('Use an unencrypted source')
        if not 1<=page<=len(document):raise ValueError('Invalid page number')
        selected=document[page-1]
        if selected.rect.width*selected.rect.height*(dpi/72)**2>80_000_000:raise ValueError('Raster too large')
        pix=selected.get_pixmap(matrix=pymupdf.Matrix(dpi/72,dpi/72),alpha=False)
        write_new(output,pix.tobytes('png'))
    save_json(str(output)+'.source.json',dict(sourceSha256=sha.lower(),rasterSha256=digest(read(output)),dpi=dpi,page=page,renderer='PyMuPDF '+pymupdf.VersionBind))

def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__); sub=parser.add_subparsers(dest='command',required=True)
    p=sub.add_parser('inspect');p.add_argument('gerber');p.add_argument('--placements');p.add_argument('--output',required=True)
    p=sub.add_parser('production-audit');p.add_argument('--production',required=True);p.add_argument('--submitted',required=True);p.add_argument('--output',required=True)
    p=sub.add_parser('fetch');p.add_argument('--url',required=True);p.add_argument('--sha256',required=True);p.add_argument('--output',required=True)
    p=sub.add_parser('rasterize');p.add_argument('source');p.add_argument('--sha256',required=True);p.add_argument('--dpi',type=int,default=300);p.add_argument('--page',type=int,default=1);p.add_argument('--output',required=True)
    p=sub.add_parser('prepare-pdf');p.add_argument('source');p.add_argument('--recipe',required=True);p.add_argument('--dpi',type=int,default=300);p.add_argument('--output',required=True)
    p=sub.add_parser('crop');p.add_argument('source');p.add_argument('--sha256',required=True);p.add_argument('--box',nargs=4,type=int,required=True);p.add_argument('--output',required=True)
    p=sub.add_parser('fit');p.add_argument('--source',required=True);p.add_argument('--landmarks',required=True);p.add_argument('--gerber',required=True);p.add_argument('--placements',required=True);p.add_argument('--tolerance-mm',type=float,default=.35);p.add_argument('--output',required=True)
    p=sub.add_parser('pick');p.add_argument('--source',required=True);p.add_argument('--gerber',required=True);p.add_argument('--placements',required=True);p.add_argument('--output',required=True)
    p=sub.add_parser('prepare');p.add_argument('--source',required=True);p.add_argument('--profile',required=True);p.add_argument('--gerber',required=True);p.add_argument('--placements',required=True);p.add_argument('--dpi',type=int,default=300);p.add_argument('--output',required=True)
    p=sub.add_parser('package');p.add_argument('--baseline',required=True);p.add_argument('--candidate',required=True);p.add_argument('--output',required=True)
    p=sub.add_parser('verify');p.add_argument('--baseline',required=True);p.add_argument('--candidate',required=True);p.add_argument('--output',required=True)
    a=parser.parse_args(argv)
    try:
        if a.command=='inspect':
            files=archive(a.gerber);result=dict(schema=1,archiveSha256=digest(read(a.gerber)),board=board(a.gerber),members=[dict(name=n,bytes=len(d),sha256=digest(d),colorArtwork=Path(n).suffix.upper() in COLOR) for n,d in sorted(files.items())])
            if a.placements:result['ledsMm']=placements(a.placements);result['placementSha256']=digest(read(a.placements))
            save_json(a.output,result)
        elif a.command=='production-audit':save_json(a.output,production_audit(a.production,a.submitted))
        elif a.command=='fetch':fetch(a.url,a.sha256,a.output)
        elif a.command=='rasterize':rasterize(a.source,a.output,a.dpi,a.page,a.sha256)
        elif a.command=='prepare-pdf':prepare_pdf(a.source,a.recipe,a.output,a.dpi)
        elif a.command=='crop':crop(a.source,a.output,a.box,a.sha256)
        elif a.command=='fit':save_json(a.output,fit_profile(a.source,a.landmarks,a.gerber,a.placements,a.tolerance_mm))
        elif a.command=='pick':pick(a.source,a.gerber,a.placements,a.output)
        elif a.command=='prepare':prepare(a.source,a.profile,a.gerber,a.placements,a.output,a.dpi)
        elif a.command=='package':package(a.baseline,a.candidate,a.output)
        elif a.command=='verify':
            report=compare(a.baseline,a.candidate);save_json(a.output,report)
            if not report['nonArtworkIntegrityPassed']:return 2
        print('Completed:',a.output);return 0
    except ImportError as e:
        print('ERROR: Install host dependencies with pip install -r tools/overlay/requirements.txt:',e,file=sys.stderr);return 1
    except (ValueError,KeyError,TypeError,OverflowError,OSError,zipfile.BadZipFile,ET.ParseError) as e:
        print('ERROR:',e,file=sys.stderr);return 1
if __name__=='__main__':raise SystemExit(main())
