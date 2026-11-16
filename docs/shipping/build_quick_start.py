"""Build Avi's one-page, US Letter packing guide from guide_content.json.
Document-only tools; no firmware libraries. See README.md for dependencies.
"""
from pathlib import Path
import html,json,os,re,sys,tempfile
try:
 import reportlab
except ImportError:
 sys.path.insert(0,str(Path(os.environ.get('BROOKLYN_PDF_DEPS',Path(tempfile.gettempdir())/'brooklyn-shipping-pdf-deps'))))
from reportlab.pdfgen import canvas
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.platypus import Paragraph
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
import qrcode
from pypdf import PdfReader
import pymupdf
ROOT=Path(__file__).resolve().parent
DATA=json.loads((ROOT/'guide_content.json').read_text(encoding='utf-8-sig'))
FONT=Path(os.environ.get('BROOKLYN_PDF_FONT_DIR',r'C:\Windows\Fonts'))
pdfmetrics.registerFont(TTFont('Guide',str(FONT/'segoeui.ttf')))
pdfmetrics.registerFont(TTFont('Guide-Bold',str(FONT/'segoeuib.ttf')))
pdfmetrics.registerFontFamily('Guide',normal='Guide',bold='Guide-Bold',italic='Guide',boldItalic='Guide-Bold')
NAVY=colors.HexColor('#17365D');BLUE=colors.HexColor('#205C9F');TEXT=colors.HexColor('#17283D');MUTED=colors.HexColor('#516174');PALE=colors.HexColor('#EDF3FA');LINE=colors.HexColor('#CAD7E5')
W,H=612,792;L,R=36,576
c=canvas.Canvas(str(ROOT/'Brooklyn_Subway_Quick_Start.pdf'),pagesize=(W,H),pageCompression=1)
c.setTitle('Brooklyn Subway - Quick Start for Avi Yao');c.setAuthor('Joel Goldwein')
style=ParagraphStyle('body',fontName='Guide',fontSize=11,leading=14,textColor=TEXT,spaceAfter=0)
small=ParagraphStyle('small',parent=style,fontSize=8.5,leading=11,textColor=MUTED)
boxes=[]
def text(txt,x,top,width,sty=style):
 p=Paragraph(txt,sty);_,height=p.wrap(width,700)
 if top-height<30:raise ValueError(f'Content below printable area: {txt[:40]} at {top-height}')
 p.drawOn(c,x,top-height);boxes.append((txt,x,top-height,width,height));return top-height

def label(txt,x,baseline,size=10,color=BLUE):
 c.setFillColor(color);c.setFont('Guide-Bold',size);c.drawString(x,baseline,txt)

def plain_markup(s):
 s=s.replace('<br/>','\n').replace('<b>','**').replace('</b>','**');return html.unescape(s)

# Header
c.setFillColor(NAVY);c.circle(52,736,16,fill=1,stroke=0)
c.setFillColor(colors.white);c.setFont('Guide-Bold',10);c.drawCentredString(52,732,'BK')
label(DATA['title'],80,738,25,NAVY)
c.setFont('Guide',10.5);c.setFillColor(MUTED);c.drawString(80,721,DATA['subtitle'])
label('QUICK START',R-76,744,8.5,MUTED)
c.setStrokeColor(LINE);c.line(L,712,R,712)
label('SET UP ONCE - NO APP OR ACCOUNT NEEDED',L,694,9.5)
y=680
for i,step in enumerate(DATA['steps'],1):
 c.setFillColor(PALE);c.circle(46,y-8,10,fill=1,stroke=0)
 c.setFont('Guide-Bold',10);c.setFillColor(BLUE);c.drawCentredString(46,y-11,str(i))
 label(step['title'],64,y-9,11)
 y=text(step['text'],64,y-18,512)-9
# Write-in field: private addresses never prefilled.
c.setFillColor(PALE);c.roundRect(L,y-26,R-L,26,5,fill=1,stroke=0)
label('MY MAP WEB ADDRESS:',L+10,y-17,9,MUTED)
c.setStrokeColor(colors.HexColor('#8095AE'));c.line(L+132,y-20,R-12,y-20)
y-=43
label('EVERYDAY CONTROLS',L,y,9.5);y-=14
colw=258;x2=318

def controls(items,x,top):
 yy=top
 for item in items:
  label(item['title'],x,yy-9,9.5)
  yy=text(item['text'],x,yy-16,colw)-6
 return yy
bottom=min(controls(DATA['left_controls'],L,y),controls(DATA['right_controls'],x2,y))
y=bottom-8
# Explicit color labels make this useful in grayscale too.
label('READ THE LIGHTS',L,y,9.5);y-=17
for i,item in enumerate(DATA['colors']):
 x=L+i*137
 c.setFillColor(colors.HexColor(item['hex']));c.circle(x+4,y-3,4,fill=1,stroke=0)
 label(item['name'],x+14,y-6,9,TEXT)
 text(item['text'],x+14,y-11,120,small)
y-=29
y=text(DATA['map_note'],L,y,R-L,small)-8
# Persistent update warning, high contrast in color or black-and-white.
c.setFillColor(colors.HexColor('#FFF3CD'));c.roundRect(L,y-25,R-L,25,5,fill=1,stroke=0)
label(DATA['warning'],L+10,y-16,9.5,TEXT)
y-=38
# Help + QR: deliberately quiet, no sender's private network details.
label(DATA['help_title'],L,y,11,NAVY)
helpbottom=text(DATA['help_text'],L,y-8,424,ParagraphStyle('help',parent=style,fontSize=9.2,leading=12))
helpbottom=text(DATA['quiet_note'],L,helpbottom-6,424,small)
qr=qrcode.QRCode(error_correction=qrcode.constants.ERROR_CORRECT_M,box_size=10,border=4)
qr.add_data(DATA['guide_url']);qr.make(fit=True);qr.make_image(fill_color='black',back_color='white').save(ROOT/'guide_qr.png')
matrix=qr.get_matrix();size=80;unit=size/len(matrix);qx=R-size;qy=y-82
if qy<35:raise ValueError(f'QR too low: {qy}')
c.setFillColor(colors.white);c.rect(qx,qy,size,size,fill=1,stroke=0)
c.setFillColor(colors.black)
for row,values in enumerate(matrix):
 for col,on in enumerate(values):
  if on:c.rect(qx+col*unit,qy+(len(matrix)-row-1)*unit,unit,unit,fill=1,stroke=0)
label('FULL GUIDE',qx+7,y+2,8,MUTED)
c.linkURL(DATA['guide_url'],(qx,qy,qx+size,qy+size),relative=0,thickness=0)
footer=min(helpbottom,qy)-9
c.setFillColor(MUTED);c.setFont('Guide',8);c.drawString(L,footer,DATA['repository_label'])
c.linkURL('https://'+DATA['repository_label'],(L,footer-1,L+250,footer+10),relative=0,thickness=0)
c.showPage();c.save()
# Friendly Markdown companion, generated from exactly the same text.
lines=['# '+DATA['title'],'',DATA['subtitle'],'','## First time: connect to Wi-Fi','']
for i,s in enumerate(DATA['steps'],1):lines += [f"{i}. **{s['title']}.** {plain_markup(s['text'])}"]
lines += ['','My map web address: ______________________________','','## Everyday controls','']
for item in DATA['left_controls']+DATA['right_controls']:lines += ['### '+item['title'],'',plain_markup(item['text']),'']
lines += ['## Read the lights','']+[f"- **{x['name'].title()}:** {x['text']}." for x in DATA['colors']]
lines += ['',DATA['map_note'],'','**'+DATA['warning']+'**','','## Need help?','',plain_markup(DATA['help_text']),'',DATA['quiet_note'],'',f"[Full guide]({DATA['guide_url']})"]
(ROOT/'Brooklyn_Subway_Quick_Start.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
# Render the finished PDF, then check page geometry and text.
pdf=PdfReader(ROOT/'Brooklyn_Subway_Quick_Start.pdf');assert len(pdf.pages)==1
assert list(pdf.pages[0].mediabox)==[0,0,612,792]
extracted=pdf.pages[0].extract_text()
for required in ['BrooklynMap-Avi','192.168.4.1','10 seconds','Do not unplug power','goldwein@gmail.com','3 seconds']:
 assert required in extracted,required
for forbidden in ['10.1.1.27','10.1.1.56','tsunami2','0.2.78']:
 assert forbidden not in extracted,forbidden
assert 'Wi-Fi' in extracted and 'MY MAP WEB ADDRESS' in extracted
out=pymupdf.open(ROOT/'Brooklyn_Subway_Quick_Start.pdf');out[0].get_pixmap(matrix=pymupdf.Matrix(2,2)).save(ROOT/'Brooklyn_Subway_Quick_Start_preview.png')
report={'pageCount':1,'pageSize':'US Letter (612 x 792 pt)','guideUrl':DATA['guide_url'],'qrModulesIncludingQuietZone':len(matrix),'qrSizePoints':80,'bodyFontPoints':11,'helpFontPoints':9.2,'footerBottomPoints':footer,'textWords':len(extracted.split()),'privateNetworkDetailsExcluded':True,'renderedWith':'PyMuPDF 1.26.5','checkedRequiredText':True}
try:
 import zxingcpp
 from PIL import Image
 decoded=zxingcpp.read_barcode(Image.open(ROOT/'Brooklyn_Subway_Quick_Start_preview.png'))
 assert decoded and decoded.text==DATA['guide_url'], 'Rendered PDF QR did not decode to the guide'
 report['renderedQrDecodedToExpectedUrl']=True
except ImportError:
 report['renderedQrDecodedToExpectedUrl']='Install zxing-cpp to run this check'
(ROOT/'validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
