"""Calendar source assets and review previews. No TouchGFX installation.

Render glyphs at 6x and downsample once to native 480x480 resolution.
Each sprite has <=256 RGBA colors without dithering, and clear edges.
Rotated copies follow the project's 90-degree-left source-asset convention.
Preview dates/holidays are illustrative, never firmware calendar data.
"""
from pathlib import Path
import calendar
import json
import math

import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter

BASE = Path(__file__).resolve().parents[1]
OUT = BASE / 'gen/calendar'
ROT = BASE / 'assets_rotacionados/calendar'
PREVIEW = BASE / 'preview'
S = 6
FONT = BASE / 'fonts/Poppins-Medium.ttf'
WHITE = (235, 248, 255)
RED = (255, 83, 91)
MUTED = (100, 123, 140)
BLUE = (50, 208, 255)
YELLOW = (255, 209, 57)
COLORS = {'normal': WHITE, 'weekend': RED, 'muted': MUTED,
          'holiday': (21, 28, 31)}


def quantize(im):
    original = np.asarray(im)
    arr = np.asarray(im.quantize(colors=255, method=Image.Quantize.FASTOCTREE,
                                dither=Image.Dither.NONE).convert('RGBA')).copy()
    arr[original[..., 3] == 0] = 0
    return Image.fromarray(arr)


def text(value, size, color=WHITE, cell=None):
    font = ImageFont.truetype(str(FONT), size*S)
    box = font.getbbox(value)
    width = math.ceil((box[2]-box[0])/S)+8
    height = math.ceil((box[3]-box[1])/S)+8
    width, height = cell or (width, height)
    im = Image.new('RGBA', (width*S, height*S))
    draw = ImageDraw.Draw(im)
    x = (width*S-(box[2]-box[0]))/2-box[0]
    y = (height*S-(box[3]-box[1]))/2-box[1]
    draw.text((x,y),value,font=font,fill=(*color,255))
    return quantize(im.resize((width,height),Image.Resampling.LANCZOS))


def marker(holiday=False, ring_only=False):
    n=44
    im=Image.new('RGBA',(n*S,n*S))
    draw=ImageDraw.Draw(im)
    if holiday:
        draw.ellipse((7*S,7*S,37*S,37*S), fill=(*YELLOW,255))
    else:
        draw.ellipse((6*S,6*S,38*S,38*S),fill=None if ring_only else (9,77,113,185),
                     outline=(*BLUE,255),width=2*S)
        glow=im.filter(ImageFilter.GaussianBlur(2*S))
        glow.alpha_composite(im)
        im=glow
    arr=np.asarray(im.resize((n,n),Image.Resampling.LANCZOS)).copy()
    arr[[0,-1],:]=0
    arr[:,[0,-1]]=0
    return quantize(Image.fromarray(arr))


def background():
    im=Image.new('RGBA',(480*S,480*S),(0,0,0,255))
    draw=ImageDraw.Draw(im)
    for r in range(236,0,-1):
        k=1-r/236
        draw.ellipse(((240-r)*S,(240-r)*S,(240+r)*S,(240+r)*S),
                     fill=(3,int(11+4*k),int(19+6*k),255))
    # The existing interface owns the perimeter ring; do not duplicate it here.
    return quantize(im.resize((480,480),Image.Resampling.LANCZOS))


def assets():
    result={'cal_background':background(),'cal_today':marker(),
            'cal_today_holiday':marker(ring_only=True),
            'cal_holiday':marker(True)}
    for c in '0123456789:-':
        result[f'cal_clock_{ord(c):02x}']=text(c,72,cell=(48,88) if c != ':' else (22,88))
    for kind,color in COLORS.items():
        for day in range(1,32):
            result[f'cal_day_{kind}_{day:02}']=text(str(day),21,color,(34,32))
    for month in range(1,13):
        result[f'cal_month_{month:02}']=text(calendar.month_name[month],23)
    for digit in '0123456789':
        result[f'cal_year_{digit}']=text(digit,23,cell=(23,34))
    for i,day in enumerate(['Mon','Tue','Wed','Thu','Fri','Sat','Sun']):
        result[f'cal_weekday_{i}']=text(day,13, RED if i>=5 else (146,196,221),(42,24))
    result['cal_location']=text('ABU DHABI',10,(113,173,195))
    lines=[text(label,9,(113,173,195)) for label in ('LEFT / RIGHT MONTH','HOLD CLOSE TO GO BACK')]
    hint=Image.new('RGBA',(max(line.width for line in lines),max(line.height for line in lines)+15))
    for row,line in enumerate(lines):
        hint.alpha_composite(line,((hint.width-line.width)//2,row*15))
    result['cal_hint']=quantize(hint)
    for key, label in [('sync','WAITING FOR WI-FI TIME'),
                       ('offline','OFFLINE - SAVED HOLIDAYS'),
                       ('loading','HOLIDAYS UNAVAILABLE'),
                       ('estimated','HOLIDAYS INCLUDE ESTIMATES'),
                       ('stale','HOLIDAYS NEED UPDATE'),
                       ('ready','PUBLIC HOLIDAYS - UAE')]:
        result['cal_status_'+key]=text(label,9,(113,173,195))
    for name,direction in [('left',-1),('right',1)]:
        im=Image.new('RGBA',(24*S,28*S))
        d=ImageDraw.Draw(im)
        d.line([(int((12-direction*3)*S),7*S),(int((12+direction*3)*S),14*S),
                (int((12-direction*3)*S),21*S)],fill=(*WHITE,255),width=2*S)
        result[f'cal_arrow_{name}']=quantize(im.resize((24,28),Image.Resampling.LANCZOS))
    return result


def center(im,sprite,x,y,alpha=255):
    if alpha!=255:
        sprite=sprite.copy()
        sprite.putalpha(sprite.getchannel('A').point(lambda a:a*alpha//255))
    im.alpha_composite(sprite,(round(x-sprite.width/2),round(y-sprite.height/2)))


def compose(a,year,month,today,holidays,pulse=255,leak=False,colon=True):
    # Calendar is transparent over the existing board, at 40% brightness.
    board=Image.open(BASE.parent/'TouchGFX/assets/images/board.png').convert('RGBA').rotate(-90)
    im=Image.blend(Image.new('RGBA',board.size,(0,0,0,255)),board,0.4)
    if leak:
        im=Image.blend(im,Image.new('RGBA',im.size,(60,60,60,255)),0.12)
    center(im,a['cal_location'],240,48)
    x=240-(4*48+22)/2
    for c in '14:18':
        sp=a[f'cal_clock_{ord(c):02x}']
        if c != ':' or colon:
            center(im,sp,x+sp.width/2,106)
        x+=sp.width
    title=a[f'cal_month_{month:02}']
    width=title.width+9+4*17
    left=240-width/2
    center(im,title,left+title.width/2,170)
    for i,digit in enumerate(str(year)):
        center(im,a[f'cal_year_{digit}'],left+title.width+9+i*17+8.5,170)
    center(im,a['cal_arrow_left'],73,170)
    center(im,a['cal_arrow_right'],407,170)
    for col in range(7): center(im,a[f'cal_weekday_{col}'],90+col*50,208)
    dates=list(calendar.Calendar(0).itermonthdates(year,month))
    # Reserve six rows in every month so typography and navigation stay fixed.
    for index,date in enumerate(dates):
        col,row=index%7,index//7
        x,y=90+col*50,243+row*32
        current=date.year==year and date.month==month
        holiday=current and date.day in holidays
        if holiday: center(im,a['cal_holiday'],x,y)
        if current and date.day==today:
            center(im,a['cal_today_holiday' if holiday else 'cal_today'],x,y,pulse)
        style='muted' if not current else 'holiday' if holiday else 'weekend' if col>=5 else 'normal'
        center(im,a[f'cal_day_{style}_{date.day:02}'],x,y)
    center(im,a['cal_status_estimated'],240,427)
    center(im,a['cal_hint'],240,450)
    return im.convert('RGB')


def main():
    for folder in (OUT,ROT,PREVIEW): folder.mkdir(parents=True,exist_ok=True)
    a=assets()
    entries={}
    for name,im in a.items():
        arr=np.asarray(im)
        colors=len(np.unique(arr.reshape(-1,4),axis=0))
        assert colors<=256,(name,colors)
        if name!='cal_background':
            assert max(arr[0,:,3].max(),arr[-1,:,3].max(),arr[:,0,3].max(),arr[:,-1,3].max())==0,name
        im.save(OUT/f'{name}.png')
        im.rotate(90,expand=True).save(ROT/f'{name}.png')
        entries[name]={'size':im.size,'colors':colors}
    (OUT/'manifest.json').write_text(json.dumps(entries,indent=2))
    # Custom pixel assets bypass Designer's per-image configuration entirely.
    # Layout: pre-rotation LEFT 90 + converter layout LEFT 90 = LEFT 180.
    # Verified against ST ImageConvert 4.26.1 with an asymmetric 2x3 probe.
    # PixelDataWidget accepts ARGB8888 and delegates clipping/orientation to LCD.
    compiled=BASE/'compiled/calendar'
    compiled.mkdir(parents=True,exist_ok=True)
    header=['// Generated by gen_calendar.py.\n#pragma once\n#include <stdint.h>\n',
            'struct CalendarSprite { const uint8_t* pixels; uint16_t width, height; };\n',
            'enum CalendarAsset {\n']
    source=['// Generated by gen_calendar.py; ARGB8888 in Portrait memory layout.\n',
            '#include "CalendarAssets.hpp"\n#include <touchgfx/hal/Config.hpp>\n']
    names=sorted(a)
    for name in names:
        header.append('    '+name.upper()+',\n')
        im=a[name].rotate(180)
        rgba=np.asarray(im,dtype=np.uint32)
        words=(rgba[:,:,3]<<24)|(rgba[:,:,0]<<16)|(rgba[:,:,1]<<8)|rgba[:,:,2]
        source.append('LOCATION_PRAGMA("ExtFlashSection")\n'
            f'KEEP static const uint32_t pixels_{name}[] LOCATION_ATTRIBUTE("ExtFlashSection") = {{\n')
        flat=words.ravel()
        source.extend('    '+','.join(f'0x{v:08x}' for v in flat[i:i+12])+',\n' for i in range(0,len(flat),12))
        source.append('};\n')
    header.append('    CAL_ASSET_COUNT\n};\nextern const CalendarSprite calendarSprites[CAL_ASSET_COUNT];\n')
    source.append('const CalendarSprite calendarSprites[CAL_ASSET_COUNT] = {\n')
    for name in names:
        source.append(f'    {{ reinterpret_cast<const uint8_t*>(pixels_{name}), {a[name].width}, {a[name].height} }},\n')
    source.append('};\n')
    (compiled/'CalendarAssets.hpp').write_text(''.join(header))
    (compiled/'CalendarAssets.cpp').write_text(''.join(source))
    # Match reference for visual review; yellow dates below are samples only.
    compose(a,2026,10,14,{6,26}).save(PREVIEW/'calendar_mockup.png')
    compose(a,2026,10,14,{6,26},leak=True).save(PREVIEW/'calendar_mockup_leak.png')
    compose(a,2026,8,14,set()).save(PREVIEW/'calendar_six_rows.png')
    frames=[compose(a,2026,10,14,{6,26},int(80+175*(0.5+0.5*math.cos(i*2*math.pi/30))),colon=i<20) for i in range(40)]
    frames[0].save(PREVIEW/'calendar_mockup.gif',save_all=True,append_images=frames[1:],duration=50,loop=0)
    print(f'{len(a)} assets validated: <=256 colors, clear sprite edges, rotated source copies.')
    print(f'Estimated L8 pixels + palettes: {sum(im.width*im.height+1024 for im in a.values())/1024:.0f} KiB')
    print(f'Runtime ARGB8888 pixels in QSPI: {sum(im.width*im.height*4 for im in a.values())/1024:.0f} KiB')


if __name__=='__main__': main()
