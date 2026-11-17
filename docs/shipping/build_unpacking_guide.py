# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Build the one-page unpacking insert and matching phone-readable text."""
from pathlib import Path
import html,json,os,sys,tempfile
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
from pypdf import PdfReader
import pymupdf,qrcode
ROOT=Path(__file__).resolve().parent
D=json.loads((ROOT/'unpacking_content.json').read_text(encoding='utf-8-sig'))
FONT=Path(os.environ.get('BROOKLYN_PDF_FONT_DIR',r'C:\Windows\Fonts'))
for name,file in [('Unpack','segoeui.ttf'),('Unpack-Bold','segoeuib.ttf')]:pdfmetrics.registerFont(TTFont(name,str(FONT/file)))
pdfmetrics.registerFontFamily('Unpack',normal='Unpack',bold='Unpack-Bold',italic='Unpack',boldItalic='Unpack-Bold')
NAVY=colors.HexColor('#17365D');BLUE=colors.HexColor('#205C9F');TEXT=colors.HexColor('#17283D');MUTED=colors.HexColor('#516174')
L,R=36,576;W,H=612,792
c=canvas.Canvas(str(ROOT/'Brooklyn_Subway_Unpacking_Guide.pdf'),pagesize=(W,H),pageCompression=1)
c.setTitle('Brooklyn Subway - Unpacking and First Startup');c.setAuthor('Joel Goldwein')
body=ParagraphStyle('body',fontName='Unpack',fontSize=10.5,leading=13.4,textColor=TEXT)
small=ParagraphStyle('small',parent=body,fontSize=9.2,leading=11.6,textColor=MUTED)
boxes=[]
def paragraph(text,x,top,width,style=body,draw=True):
 p=Paragraph(text,style);_,height=p.wrap(width,720)
 if top-height<30:raise ValueError('Content too low: '+str(top-height)+' '+text[:45])
 if draw:p.drawOn(c,x,top-height);boxes.append((x,top-height,width,height))
 return top-height

def label(text,x,y,size=11,color=BLUE):
 c.setFillColor(color);c.setFont('Unpack-Bold',size);c.drawString(x,y,text)

label(D['title'],L,742,26,NAVY)
c.setFillColor(MUTED);c.setFont('Unpack',10.5);c.drawString(L,723,D['subtitle'])
label(D['heading'],L,699,10)
c.setStrokeColor(colors.HexColor('#CAD7E5'));c.line(L,712,R,712)
y=paragraph(D['intro'],L,689,R-L,small)-13
for i,step in enumerate(D['steps'],1):
 c.setFillColor(colors.HexColor('#EDF3FA'));c.circle(46,y-6,10,fill=1,stroke=0)
 c.setFillColor(BLUE);c.setFont('Unpack-Bold',10);c.drawCentredString(46,y-9,str(i))
 label(step['title'],64,y-9)
 y=paragraph(step['text'],64,y-18,512)-12

# Strong warning remains readable on color or monochrome printers.
warning=Paragraph(D['update_text'],body);_,height=warning.wrap(R-L-20,720)
boxHeight=height+37
if y-boxHeight<30:raise ValueError('Update warning too low')
c.setFillColor(colors.HexColor('#FFF3CD'));c.roundRect(L,y-boxHeight,R-L,boxHeight,6,fill=1,stroke=0)
label(D['update_title'],L+10,y-15,10,TEXT)
warning.drawOn(c,L+10,y-24-height)
boxes.append((L,y-boxHeight,R-L,boxHeight));y-=boxHeight+18
label(D['after_title'],L,y-2,11,NAVY)
y=paragraph(D['after_text'],L,y-11,R-L)-10
y=paragraph(D['care'],L,y,R-L,small)-15
label('Need help?',L,y-2,11,NAVY)
helpBottom=paragraph(D['help'],L,y-12,424,small)
qr=qrcode.QRCode(error_correction=qrcode.constants.ERROR_CORRECT_M,box_size=10,border=4)
qr.add_data(D['guide_url']);qr.make(fit=True);matrix=qr.get_matrix()
size=72;unit=size/len(matrix);qx=R-size;qy=y-78
if qy<40:raise ValueError('QR too low: '+str(qy))
c.setFillColor(colors.white);c.rect(qx,qy,size,size,fill=1,stroke=0)
c.setFillColor(colors.black)
for row,values in enumerate(matrix):
 for col,on in enumerate(values):
  if on:c.rect(qx+col*unit,qy+(len(matrix)-row-1)*unit,unit,unit,fill=1,stroke=0)
label('QUICK START',qx-1,y+2,8,MUTED)
c.linkURL(D['guide_url'],(qx,qy,qx+size,qy+size),relative=0,thickness=0)
footer=min(qy,helpBottom)-12
if footer<30:raise ValueError('Footer too low: '+str(footer))
c.setFillColor(MUTED);c.setFont('Unpack',8);c.drawString(L,footer,D['repository_label'])
c.linkURL('https://'+D['repository_label'],(L,footer-1,L+260,footer+10),relative=0,thickness=0)
c.showPage();c.save()

def plain(s):return html.unescape(s.replace('<b>','**').replace('</b>','**'))
lines=['# '+D['title']+' - Unpacking and First Startup','',D['subtitle'],'',D['intro'],'']
for i,step in enumerate(D['steps'],1):lines += [str(i)+'. **'+step['title']+'.** '+plain(step['text']),'']
lines+=['## '+D['update_title'],'',plain(D['update_text']),'','## '+D['after_title'],'',plain(D['after_text']),'',plain(D['care']),'','## Need help?','',plain(D['help']),'','[Quick Start]('+D['guide_url']+')']
(ROOT/'Brooklyn_Subway_Unpacking_Guide.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
pdf=PdfReader(ROOT/'Brooklyn_Subway_Unpacking_Guide.pdf');assert len(pdf.pages)==1
assert list(pdf.pages[0].mediabox)==[0,0,612,792]
text=" ".join(pdf.pages[0].extract_text().split())
for phrase in ['Do not unplug power','Update complete','BrooklynMap-Avi','192.168.4.1','direct sunlight','USB-C','magnet','antenna','5 V, 3 A']:
 assert phrase in text,phrase
for secret in ['10.1.1.27','10.1.1.56','tsunami2']:assert secret not in text
render=pymupdf.open(ROOT/'Brooklyn_Subway_Unpacking_Guide.pdf')
render[0].get_pixmap(matrix=pymupdf.Matrix(2,2)).save(ROOT/'Brooklyn_Subway_Unpacking_Guide_preview.png')
spans=[s for block in render[0].get_text('dict')['blocks'] if 'lines' in block for line in block['lines'] for s in line['spans']]
assert any('CARE:' in s['text'] and s['flags']&16 for s in spans),'CARE label must be bold'
report={'pageCount':1,'pageSize':'US Letter','bodyFontPoints':10.5,'footerBottomPoints':footer,'wordCount':len(text.split()),'requiredTextChecked':True,'careLabelBold':True,'privateNetworkDetailsExcluded':True}
try:
 import zxingcpp
 from PIL import Image
 decoded=zxingcpp.read_barcode(Image.open(ROOT/'Brooklyn_Subway_Unpacking_Guide_preview.png'))
 assert decoded and decoded.text==D['guide_url'];report['renderedQrDecoded']=True
except ImportError:report['renderedQrDecoded']='zxing-cpp unavailable'
(ROOT/'unpacking_validation.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report,indent=2))
