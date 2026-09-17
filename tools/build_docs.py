#!/usr/bin/env python3
"""Build AERIS PDFs/SVG/PNG from the shared firmware renderer.
Dependencies: reportlab, Pillow, PyMuPDF; g++ unless --frames is supplied.
"""
from pathlib import Path
import argparse, math, subprocess, tempfile
from xml.sax.saxutils import escape
from reportlab.pdfgen import canvas
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.graphics.shapes import Drawing, Rect, Line, Circle, String, PolyLine
from reportlab.graphics import renderPDF, renderSVG
from reportlab.lib import colors
from reportlab.platypus import Paragraph
from reportlab.lib.styles import ParagraphStyle
from PIL import Image, ImageDraw, ImageFont
import fitz

ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / 'docs'
W, H = 1200, 840
INK = '#132339'; MUTED = '#55677d'; TEAL = '#008d85'; LIGHT = '#eef5f7'
NAVY = '#081522'; PALE = '#abc2cc'; CYAN = '#54e5cf'
RED = '#ca4251'; BLUE = '#287aca'; GOLD = '#aa790d'; PURPLE = '#8657b9'; GROUND = '#536276'
FONT_DIR = Path('/usr/share/fonts/truetype/dejavu')
for name, fname in [('Aeris','DejaVuSans.ttf'),('AerisBold','DejaVuSans-Bold.ttf'),('AerisMono','DejaVuSansMono.ttf')]:
    p = FONT_DIR / fname
    if p.exists(): pdfmetrics.registerFont(TTFont(name,str(p)))
    else:
        from reportlab.pdfbase.pdfmetrics import Font
        pdfmetrics.registerFont(Font(name, {'Aeris':'Helvetica','AerisBold':'Helvetica-Bold','AerisMono':'Courier'}[name], 'WinAnsiEncoding'))

def color(v): return colors.toColor(v)

class Schematic:
    def __init__(self): self.d = Drawing(W,H)
    def rect(self,x,y,w,h,fill='white',stroke='#d3dfe6',r=8,lw=1):
        self.d.add(Rect(x,H-y-h,w,h,rx=r,ry=r,fillColor=color(fill) if fill else None,strokeColor=color(stroke) if stroke else None,strokeWidth=lw))
    def text(self,x,y,t,size=14,fill=INK,bold=False,align='start'):
        self.d.add(String(x,H-y,t,fontName='AerisBold' if bold else 'Aeris',fontSize=size,fillColor=color(fill),textAnchor=align))
    def line(self,*points,stroke=INK,lw=2):
        pts=[]
        for x,y in points:pts.extend([x,H-y])
        self.d.add(PolyLine(pts,strokeColor=color(stroke),strokeWidth=lw,fillColor=None))
    def dot(self,x,y,fill=INK,r=3):
        self.d.add(Circle(x,H-y,r,fillColor=color(fill),strokeColor=None))
    def ground(self,x,y):
        self.line((x,y),(x,y+8),stroke=GROUND)
        for off,width in [(8,12),(13,8),(18,4)]:self.line((x-width,y+off),(x+width,y+off),stroke=GROUND,lw=1.8)
    def resistor_h(self,x1,x2,y,label,stroke=GOLD):
        self.line((x1,y),(x1+8,y),stroke=stroke)
        self.rect(x1+8,y-6,x2-x1-16,12,fill='white',stroke=stroke,r=0,lw=1.8)
        self.line((x2-8,y),(x2,y),stroke=stroke)
        self.text((x1+x2)/2,y-14,label,12,stroke,align='middle')
    def resistor_v(self,x,y1,y2):
        self.line((x,y1),(x,y1+8),stroke=GOLD)
        self.rect(x-6,y1+8,12,y2-y1-16,fill='white',stroke=GOLD,r=0,lw=1.8)
        self.line((x,y2-8),(x,y2),stroke=GOLD)

def circuit():
    s=Schematic(); s.rect(0,0,W,H,fill='white',stroke=None,r=0)
    s.text(46,48,'AERIS / CIRCUIT',30,bold=True)
    s.text(46,77,'ESP32 DevKit V1 + MQ135 + BME280 + SSD1306 + four-terminal push button',16,MUTED)
    s.text(1154,45,'REV 1.0',14,TEAL,bold=True,align='end')
    s.line((46,96),(1154,96),stroke='#d3dfe6',lw=1)
    legend=[(52,RED,'5V'),(160,TEAL,'3V3'),(275,BLUE,'SDA'),(380,PURPLE,'SCL / BUTTON'),(584,GOLD,'ANALOG'),(753,GROUND,'GND')]
    for x,col,txt in legend:
        s.line((x,121),(x+25,121),stroke=col,lw=3);s.text(x+34,126,txt,13)
    s.text(1150,126,'Only junction dots join crossing wires.',12,MUTED,align='end')
    # MCU card; header/pin positions are logical, not a physical board footprint.
    s.rect(460,185,270,440,fill=LIGHT,stroke='#8da7b7',r=14,lw=1.5)
    s.rect(498,370,194,75,fill=INK,stroke=None,r=7)
    s.text(595,397,'ESP32-WROOM-32',17,'white',True,'middle')
    s.text(595,422,'DEVKIT V1',14,PALE,False,'middle')
    for y,label in [(225,'5V / VIN*'),(345,'GPIO34 / ADC1'),(475,'GPIO27')]:
        s.dot(460,y);s.text(474,y+5,label,14,bold=True)
    for y,label in [(215,'3V3'),(260,'GPIO21 / SDA'),(310,'GPIO22 / SCL'),(355,'GND')]:
        s.dot(730,y);s.text(716,y+5,label,14,bold=True,align='end')
    s.text(595,526,'INTERNAL PULL-UP',12,MUTED,align='middle')
    s.text(595,549,'GPIO27: HIGH at rest',13,MUTED,align='middle')
    s.text(595,570,'LOW when pressed',13,MUTED,align='middle')
    s.text(595,607,'MICRO-USB',14,bold=True,align='middle')
    # MQ module and divider.
    s.rect(48,189,176,179,fill='#fff9e9',stroke='#debf6d')
    s.text(64,215,'MQ135 MODULE',16,bold=True)
    s.text(64,247,'VCC',14);s.text(64,285,'AOUT / AO',14);s.text(64,330,'GND',14)
    s.dot(224,235);s.dot(224,280);s.dot(224,325)
    s.line((224,235),(360,235),(360,225),(460,225),stroke=RED)
    s.line((224,280),(265,280),stroke=GOLD)
    s.resistor_h(265,335,280,'R1 15k')
    s.line((335,280),(430,280),(430,345),(460,345),stroke=GOLD)
    s.dot(355,280,GOLD);s.dot(400,280,GOLD)
    s.resistor_v(355,300,354);s.line((355,280),(355,300),stroke=GOLD)
    s.ground(355,354)
    s.text(320,315,'R2',12,GOLD);s.text(317,332,'10k',12,GOLD)
    s.line((400,280),(400,318),stroke=GOLD)
    s.line((390,318),(410,318),stroke=GOLD)
    s.line((390,324),(410,324),stroke=GOLD)
    s.line((400,324),(400,354),stroke=GROUND);s.ground(400,354)
    s.text(383,253,'C1',11,GOLD);s.text(383,269,'100nF',10,GOLD)
    s.line((224,325),(245,325),(245,354),stroke=GROUND);s.ground(245,354)
    s.text(52,393,'DOUT / DO: leave unconnected.',13,MUTED)
    s.text(263,398,'GPIO34 = 0.4 x AOUT',13,GOLD,True)
    s.text(263,419,'5.0V AOUT becomes 2.0V.',12,MUTED)
    # Shared I2C trunks. Crossings have no electrical joint unless dotted.
    for x,y,ys,col in [(770,215,(195,445),TEAL),(790,260,(235,485),BLUE),(810,310,(275,525),PURPLE),(830,355,(315,565),GROUND)]:
        s.line((730,y),(x,y),stroke=col)
        s.line((x,ys[0]),(x,ys[1]),stroke=col)
        s.dot(x,y,col)
        for end in ys:s.line((x,end),(860,end),stroke=col);s.dot(x,end,col)
    s.line((830,565),(830,610),stroke=GROUND);s.ground(830,610)
    for y,title,sub in [(160,'BME280','TEMP / HUMIDITY / PRESSURE'),(410,'SSD1306 OLED','128 x 64 / 0.96 INCH / I2C')]:
        s.rect(860,y,290,198,fill=LIGHT,stroke='#aac1ce')
        s.text(1130,y+27,title,20,bold=True,align='end')
        for off,label in [(35,'VCC'),(75,'SDA'),(115,'SCL'),(155,'GND')]:
            s.dot(860,y+off);s.text(875,y+off+5,label,13)
        s.text(975,y+75,'3.3V POWER',12,TEAL,True)
        s.text(975,y+99,'0x76 / 0x77' if y==160 else '0x3C / 0x3D',14,bold=True)
        s.text(1130,y+180,sub,11,MUTED,align='end')
    # Four-pin button: electrical groups, one wire to each group.
    s.rect(48,447,330,180,fill='#f7f2fc',stroke='#c2abd9')
    s.text(64,473,'S1 / FOUR-TERMINAL BUTTON',15,bold=True)
    s.line((140,500),(140,560),stroke=PURPLE)
    s.line((280,500),(280,560),stroke=PURPLE)
    for x in (140,280):
        for y in (500,560):s.dot(x,y,PURPLE,r=4)
    s.dot(140,530,PURPLE);s.dot(280,530,PURPLE)
    s.line((140,530),(249,510),stroke=PURPLE,lw=2.2)
    s.line((262,530),(280,530),stroke=PURPLE,lw=2.2)
    s.text(109,503,'B1',12);s.text(109,564,'B2',12)
    s.text(290,491,'A1',12);s.text(290,564,'A2',12)
    s.line((460,475),(407,475),(407,500),(280,500),stroke=PURPLE)
    s.line((140,560),(140,580),stroke=GROUND);s.ground(140,580)
    s.text(176,608,'PRESS TO JOIN A + B',12,PURPLE)
    s.text(50,650,'A1-A2 and B1-B2 are permanent pairs.',13,bold=True)
    s.text(50,671,'Pair labels are logical. Verify with a meter.',12,MUTED)
    # USB feed, no dual external feed.
    s.rect(485,680,222,69,fill='#fff0f0',stroke='#ddb0b7')
    s.line((595,625),(595,680),stroke=RED,lw=3)
    s.text(596,707,'5V USB SUPPLY',17,RED,True,'middle')
    s.text(596,731,'1A available / one source',12,MUTED,False,'middle')
    s.text(755,663,'I2C PULL-UPS',13,TEAL,True)
    s.text(755,685,'Assumed on breakouts, tied to 3V3.',12,MUTED)
    s.text(755,704,'If absent: 4.7k SDA-to-3V3 and SCL-to-3V3.',12,MUTED)
    s.text(755,731,'All GND symbols are the same net.',13,bold=True)
    s.line((46,770),(1154,770),stroke='#d3dfe6',lw=1)
    s.text(46,793,'*Verify USB 5V is available on your board pin and its current path is adequate. Never feed MQ135 AOUT directly to a GPIO.',12,RED)
    s.text(46,816,'Logical wiring schematic, not PCB layout. Check actual module labels. R1/R2: 1%, 1/4W. C1: near GPIO34. Power off before rewiring.',11,MUTED)
    return s.d

class Guide:
    def __init__(self,path):
        self.c=canvas.Canvas(str(path),pagesize=(W,H));self.c.setTitle('AERIS Air Monitor - Build Guide')
        self.c.setAuthor('AERIS Project');self.page=0
    def rect(self,x,y,w,h,fill,stroke=None,r=0):
        self.c.setFillColor(color(fill));self.c.setStrokeColor(color(stroke or fill))
        self.c.roundRect(x,H-y-h,w,h,r,fill=1,stroke=bool(stroke))
    def text(self,x,y,t,size=18,fill=INK,bold=False,font=None):
        self.c.setFillColor(color(fill));self.c.setFont(font or ('AerisBold' if bold else 'Aeris'),size)
        self.c.drawString(x,H-y,t)
    def para(self,x,y,w,t,size=17,fill=INK,leading=None):
        st=ParagraphStyle('p',fontName='Aeris',fontSize=size,leading=leading or size*1.42,textColor=color(fill))
        p=Paragraph(t,st);pw,ph=p.wrap(w,1000);p.drawOn(self.c,x,H-y-ph);return ph
    def image(self,p,x,y,w,h):self.c.drawImage(str(p),x,H-y-h,width=w,height=h,mask='auto')
    def base(self,title,subtitle):
        self.page+=1;self.rect(0,0,W,H,'white')
        self.text(50,43,'AERIS / BUILD GUIDE',13,TEAL,True)
        self.text(50,93,title,34,bold=True)
        self.text(50,126,subtitle,16,MUTED)
    def footer(self,dark=False):
        self.text(50,812,'ESP32 AIR MONITOR  /  REV 1.0  /  12 SEP 2026',11,PALE if dark else MUTED)
        self.text(1118,812,f'{self.page:02}',13,CYAN if dark else TEAL,True)
        self.c.showPage()
    def table(self,x,y,widths,rows,row_h=43,size=15):
        for i,row in enumerate(rows):
            self.rect(x,y+i*row_h,sum(widths),row_h,INK if i==0 else (LIGHT if i%2 else '#f8fafb'))
            xx=x
            for width,txt in zip(widths,row):
                self.para(xx+12,y+i*row_h+10,width-24,escape(str(txt)),size,'white' if i==0 else INK,leading=size*1.25)
                xx+=width
    def link(self,x,y,label,url,size=14):
        self.text(x,y,label,size,TEAL)
        self.c.linkURL(url,(x,H-y-3,x+pdfmetrics.stringWidth(label,'Aeris',size),H-y+size),relative=0,thickness=0)


def make_previews(frames):
    # Upscale only at integer ratios; preserves the actual 128x64 framebuffer.
    for i in [1,2,3,4,'warmup','reference']:
        Image.open(frames/f'screen-{i}.pgm').convert('RGB').resize((768,384),Image.Resampling.NEAREST).save(DOCS/f'screen-{i}.png')
    im=Image.new('RGB',(1600,1120),NAVY);d=ImageDraw.Draw(im)
    ft=str(FONT_DIR/'DejaVuSans.ttf');fb=str(FONT_DIR/'DejaVuSans-Bold.ttf')
    def font(n,b=False):return ImageFont.truetype(fb if b else ft,n)
    d.text((68,50),'AERIS / DISPLAY SYSTEM',font=font(38,True),fill=CYAN)
    d.text((68,112),'Actual firmware renderer  /  128 x 64 monochrome  /  sample data',font=font(23),fill=PALE)
    labels=['01  GAS RESPONSE','02  CLIMATE','03  SIGNAL TREND','04  SYSTEM STATUS']
    for i,label in enumerate(labels):
        x=68+(i%2)*785;y=220+(i//2)*430
        d.text((x,y-47),label,font=font(24,True),fill='white')
        frame=Image.open(DOCS/f'screen-{i+1}.png').resize((640,320),Image.Resampling.NEAREST)
        d.rounded_rectangle((x-12,y-12,x+652,y+332),radius=14,fill='#162a3b',outline='#355469',width=2)
        im.paste(frame,(x,y))
    d.text((68,1060),'TAP: NEXT SCREEN     HOLD ON GAS: REFERENCE     HOLD ELSEWHERE: DIM',font=font(21),fill=PALE)
    im.save(DOCS/'AERIS-Display-Preview.png')


def build_guide(drawing):
    g=Guide(DOCS/'AERIS-Build-Guide.pdf')
    # 01 cover
    g.page=1;g.rect(0,0,W,H,NAVY)
    for x in range(620,1200,30):
        for y in range(135,560,30):g.rect(x,y,1.4,1.4,'#214051')
    g.text(55,63,'AERIS / EMBEDDED AIR MONITOR',15,CYAN,True)
    g.text(55,183,'AIR.',96,'white',True)
    g.text(55,274,'MADE VISIBLE.',52,'white',True)
    g.para(58,315,485,'Your ESP32 project, with four purposeful screens, a one-button interface and a corrected circuit you can build.',23,PALE)
    g.rect(655,194,476,292,'#162a3b','#345066',18)
    g.image(DOCS/'screen-1.png',675,220,436,218)
    g.text(676,465,'128 x 64 OLED / FICTIONAL SAMPLE DATA',11,CYAN)
    g.rect(57,496,478,48,'#13373b',r=8)
    g.text(74,526,'TAP TO EXPLORE. HOLD TO ACT.',18,CYAN,True)
    features=[('04','OLED SCREENS'),('01','TACTILE BUTTON'),('02','SENSOR MODULES')]
    for x,(big,small) in zip([60,430,800],features):
        g.text(x,652,big,51,'white',True);g.text(x,688,small,14,CYAN,True)
    g.para(60,724,1050,'MQ135 gas response + BME280 climate. Package: Arduino sketch, PlatformIO profile, PDF + editable SVG circuit, shared-renderer previews and portable tests.',16,PALE)
    g.footer(True)
    # 02 full schematic
    g.page+=1;renderPDF.draw(drawing,g.c,0,0);g.c.showPage()
    # 03 parts + pin table
    g.base('Build the circuit with confidence.','Use the labels on your actual modules. GPIO numbers are not physical header positions.')
    g.text(50,179,'PARTS',18,TEAL,True)
    parts=[('QTY','ITEM'),('1','ESP32-WROOM-32 / DevKit V1'),('1','MQ135 module with analog output'),('1','BME280 I2C breakout'),('1','SSD1306 128x64 I2C OLED'),('1','4-terminal normally-open button'),('1 each','15k + 10k resistors, 1%, 1/4W'),('1','100nF ceramic capacitor'),('1','USB 5V supply, 1A available'),('As needed','USB data cable, wires, breadboard')]
    parts[-1]=('Set','USB data cable, wires, breadboard')
    g.table(50,198,[100,425],parts,row_h=44,size=15)
    g.text(625,179,'EXACT CONNECTIONS',18,TEAL,True)
    pins=[('MODULE / PIN','CONNECT TO'),('MQ135 VCC','Verified USB 5V / VIN rail'),('MQ135 AOUT','15k -> GPIO34 node'),('GPIO34 node','10k + 100nF to GND, in parallel'),('BME280 + OLED VCC','ESP32 3V3'),('Both SDA pins','GPIO21'),('Both SCL pins','GPIO22'),('Button pair A / pair B','GPIO27 / GND'),('All GND pins','Common ground'),('MQ135 DOUT / DO','Leave unconnected')]
    pins[3]=('GPIO34 node','10k + 100nF to GND')
    g.table(625,198,[216,309],pins,row_h=44,size=14)
    g.rect(50,672,1100,89,LIGHT,r=10)
    g.para(70,687,1060,'<b>Before connecting power:</b> check the 15k series / 10k shunt divider. The ADC node should be 0.4 x AOUT, approximately 2.0V at a 5.0V AOUT. Power the MQ135 from 5V and both I2C modules from 3V3. Keep the 100nF capacitor close to GPIO34.',17)
    g.footer()
    # 04 button
    g.base('Four legs. Two electrical pairs.','The switch joins the two groups only while you press it. Pair labels below are logical.')
    g.rect(50,170,475,359,'#f7f2fc',r=14)
    # Draw a large logical switch directly, using the same primitives.
    g.text(90,216,'PAIR B / GND',16,PURPLE,True);g.text(321,216,'PAIR A / GPIO27',16,PURPLE,True)
    c=g.c;c.setStrokeColor(color(PURPLE));c.setLineWidth(3)
    def line(x1,y1,x2,y2):c.line(x1,H-y1,x2,H-y2)
    line(151,276,151,411);line(400,276,400,411)
    line(151,344,356,303);line(372,344,400,344)
    for x in [151,400]:
        for y in [276,411]:
            c.setFillColor(color(PURPLE));c.circle(x,H-y,7,fill=1,stroke=0)
    for x,y,t in [(105,281,'B1'),(105,416,'B2'),(419,281,'A1'),(419,416,'A2')]:g.text(x,y,t,16)
    g.para(84,449,406,'A1 and A2 always connect. B1 and B2 always connect. A and B connect only when pressed.',17)
    g.text(580,189,'BUTTON ACTIONS',18,TEAL,True)
    g.table(580,211,[220,350],[('GESTURE','BEHAVIOR'),('Tap, then release','Next screen; wraps after page 4'),('Hold 2s on Gas','Capture reference for 30 seconds'),('Hold 2s elsewhere','Toggle normal / dim brightness'),('Continue holding','No repeated actions'),('Release after hold','No additional page change')],row_h=52,size=16)
    g.text(50,579,'FIND THE PAIRS WITH A MULTIMETER',18,TEAL,True)
    g.para(50,598,505,'With power off, use continuity mode. Each permanent pair beeps even without a press. Across the two groups it should beep only when pressed. Use one wire per group; duplicate legs need no extra wires.',18)
    g.para(625,598,525,'Connect one A leg to GPIO27 and one B leg to GND. INPUT_PULLUP makes the input HIGH at rest and LOW when pressed. Check the breadboard does not short the two groups. The navigation button is separate from BOOT/EN.',18)
    g.footer()
    # 05 setup
    g.base('Upload the firmware.','Arduino IDE is the simplest route. Keep all five files inside the AerisMonitor folder together.')
    steps=[('01','Install the ESP32 board package','In Arduino IDE Preferences, add the official board URL linked below. Install esp32 by Espressif Systems in Boards Manager.'),('02','Install the Adafruit libraries','Use Library Manager for SSD1306, GFX Library, BME280 Library, Unified Sensor and BusIO. Accept their dependencies.'),('03','Open, select, compile','Open AerisMonitor/AerisMonitor.ino. Choose DOIT ESP32 DEVKIT V1 and the correct port. Click Verify, then Upload.'),('04','Read the startup report','Open Serial Monitor at 115200 baud. Look for the detected OLED and BME280 addresses. Press the new GPIO27 button to switch pages.')]
    yy=181
    for num,title,body in steps:
        g.rect(50,yy,46,36,TEAL,r=6);g.text(59,yy+26,num,19,'white',True)
        g.text(115,yy+25,title,21,bold=True)
        g.para(115,yy+42,485,body,17);yy+=136
    g.rect(665,176,485,470,LIGHT,r=14)
    g.text(689,215,'BUILD PROFILE',21,TEAL,True)
    settings=[('Target','Original ESP32 / WROOM-32'),('Arduino core','2.0.17 in pinned PIO profile'),('PlatformIO','espressif32 6.9.0'),('Board ID','esp32doit-devkit-v1'),('Monitor','115200 baud'),('OLED','0x3C, then 0x3D'),('BME280','0x76, then 0x77')]
    yy=256
    for k,v in settings:
        g.text(690,yy,k,14,MUTED,True);g.text(815,yy,v,14);yy+=45
    g.para(689,576,430,'Internet access is needed to install the real board package and libraries. This package contains source, not a prebuilt firmware binary.',15,MUTED)
    g.text(666,684,'PLATFORMIO ALTERNATIVE',16,TEAL,True)
    g.text(666,715,'pio run --target upload',17,font='AerisMono')
    g.text(666,744,'pio device monitor --baud 115200',15,font='AerisMono')
    g.link(50,767,'Official ESP32 installation / board URL', 'https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html')
    g.footer()
    # 06 displays
    g.base('Four screens. One consistent HUD.','These are actual frames from Ui.h with fictional sample data. The physical OLED remains monochrome.')
    screens=[('01 / GAS RESPONSE','Signed change from your stored reference; raw AOUT and warm-up states.'),('02 / CLIMATE','Temperature, relative humidity and local absolute pressure.'),('03 / SIGNAL TREND','120 samples at 10-second intervals. Auto-scaled voltage, with gaps for missing data.'),('04 / SYSTEM STATUS','Detected display, BME status, ADC voltage, uptime and reference storage status.')]
    for i,(title,desc) in enumerate(screens):
        x=50+(i%2)*575;y=185+(i//2)*305
        g.text(x,y,title,19,TEAL,True)
        g.image(DOCS/f'screen-{i+1}.png',x,y+19,416,208)
        g.para(x,y+237,515,desc,15)
    g.footer()
    # 07 conditioning / interpretation
    g.base('Let the sensor settle. Set a reference.','The MQ135 is a broad gas-response sensor; the firmware does not invent AQI or CO2 ppm.')
    g.text(50,182,'FIRST-USE SEQUENCE',19,TEAL,True)
    steps=[('<b>Condition a fresh sensor.</b> The default timer is 49 continuous hours, chosen to exceed the manufacturer\'s 48-hour initial preheat requirement. Longer storage may require longer aging; see README and the source manual.'),('<b>Previously conditioned module?</b> Set MQ135_ALREADY_CONDITIONED to true only when that exact sensor has completed conditioning. Later restarts use a 10-minute software settling gate; wait longer if it still drifts.'),('<b>Capture clean ambient air.</b> On the Gas page, hold for 2 seconds. Keep conditions stable for 30 seconds. A capture needs at least 80 samples and no more than 3% peak-to-peak voltage spread.'),('<b>Read the result.</b> REFERENCE SAVED means it survives power loss. An unstable capture keeps the old reference. RAM ONLY means saving failed and the new reference will be lost at restart.')]
    yy=205
    for i,body in enumerate(steps):
        g.text(52,yy+19,str(i+1).zfill(2),20,TEAL,True)
        ph=g.para(98,yy,485,body,17);yy+=max(ph+26,111)
    g.rect(640,180,510,197,NAVY,r=14)
    g.text(664,217,'RELATIVE GAS RESPONSE',18,CYAN,True)
    g.text(664,259,'100 x (Vnow / Vref - 1)',23,'white',font='AerisMono')
    g.para(664,284,460,'Example: 1.82V versus a 1.625V reference displays +12%. This is an electrical change, not 12% pollution or a health classification.',17,PALE)
    g.text(641,423,'WHAT THE HARDWARE CAN TELL YOU',18,TEAL,True)
    g.para(641,443,502,'The BME280 reports climate and local pressure. The MQ135 reports a nonselective response affected by gases, humidity, temperature, warm-up and drift. No gas identification or unvalidated climate compensation is applied.',18)
    g.para(641,578,502,'The heater timer assumes continuous shared power; it cannot measure heater current. When replacing the MQ135, erase old Preferences/flash before reconditioning and capturing a new reference.',17)
    g.rect(50,722,1100,49,'#fff5e4',r=8)
    g.text(69,753,'Educational monitor. Do not use it as a certified smoke/gas alarm or test it with flames or lighter-gas spray.',16,'#73560f')
    g.footer()
    # 08 troubleshooting, verification and primary links
    g.base('Bring-up checks and troubleshooting.','Power down before moving wires. Then use the Serial report and the circuit sheet to isolate the fault.')
    rows=[('SYMPTOM','CHECK'),('Blank OLED','3V3/GND, SDA21/SCL22, 0x3C or 0x3D; confirm SSD1306, not SH1106.'),('Button does nothing','One wire from each electrical pair; GPIO27, not a numbered header position.'),('BME280 unavailable','Genuine BME280, 0x76/0x77, I2C straps and power. BMP280 cannot read humidity.'),('ADC out of range','MQ135 5V, common ground, 15k/10k orientation, 100nF and GPIO34 voltage.'),('Resets with heater on','USB supply capacity, cable voltage drop and the board\'s USB-to-VIN current path.'),('Warm-up / no reference','Allow conditioning and settling; then hold on Gas. Do not interpret raw volts as AQI.'),('Unstable reference','Wait for less drift; keep ambient conditions steady. The old reference is retained.')]
    g.table(50,172,[285,815],rows,row_h=48,size=15)
    g.text(50,601,'VERIFIED HERE',17,TEAL,True)
    g.para(50,618,520,'18 portable C++ behavioral/render checks passed. Full sketch syntax checked using desktop API stubs. Preview pixels and PDF layouts reviewed. <b>ESP32 target compilation, upload and physical sensor testing remain to be done.</b>',16)
    g.text(640,601,'PRIMARY REFERENCES / CLICK TO OPEN',17,TEAL,True)
    refs=[('Winsen MQ135 electrical data and conditioning','https://www.winsen-sensor.com/manual/mq135.html'),('Espressif ADC and calibrated millivolt API','https://docs.espressif.com/projects/arduino-esp32/en/latest/api/adc.html'),('Espressif GPIO and internal pull-ups','https://docs.espressif.com/projects/arduino-esp32/en/latest/api/gpio.html'),('Adafruit BME280 pinout and I2C addresses','https://learn.adafruit.com/adafruit-bme280-humidity-barometric-pressure-temperature-sensor-breakout/pinouts'),('PlatformIO ESP32 DevKit V1 board profile','https://docs.platformio.org/en/latest/boards/espressif32/esp32doit-devkit-v1.html')]
    for j,(label,url) in enumerate(refs):g.link(640,631+j*26,label,url,13)
    g.para(50,741,520,'See README.md for setup commands and serial controls. Bench acceptance checks are in docs/VALIDATION.md.',14,MUTED)
    g.footer();g.c.save()


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--frames',type=Path);args=parser.parse_args()
    DOCS.mkdir(exist_ok=True)
    if args.frames:
        frames=args.frames.resolve();make_previews(frames)
    else:
        with tempfile.TemporaryDirectory() as td:
            tmp=Path(td);binary=tmp/'core_tests'
            subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror','-pedantic',str(ROOT/'tests/core_tests.cpp'),'-o',str(binary)],check=True)
            subprocess.run([str(binary),str(tmp)],check=True);make_previews(tmp)
    drawing=circuit();renderPDF.drawToFile(drawing,str(DOCS/'AERIS-Circuit.pdf'),title='AERIS Air Monitor - Circuit')
    renderSVG.drawToFile(drawing,str(DOCS/'AERIS-Circuit.svg'))
    doc=fitz.open(DOCS/'AERIS-Circuit.pdf');doc[0].get_pixmap(matrix=fitz.Matrix(1.5,1.5)).save(DOCS/'AERIS-Circuit.png');doc.close()
    build_guide(drawing)
    print('Created circuit PDF/SVG/PNG, display PNGs, and eight-page build guide.')

if __name__=='__main__':main()
