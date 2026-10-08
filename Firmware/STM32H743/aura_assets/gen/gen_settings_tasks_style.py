"""Install Tasks-style Settings sprites in custom QSPI assets, outside Designer."""
from pathlib import Path
import argparse
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import gen_settings as legacy

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'aura_assets/compiled/settings_style'
PREVIEW = ROOT / 'aura_assets/preview/settings_tasks_style'
S = 6
FONT = ROOT / 'aura_assets/fonts/Poppins-Medium.ttf'
WHITE = (235, 248, 255)
BLUE = (160, 205, 238)
ROWS = [('Wi-Fi network', 'Wi-Fi', 'wifi', (75,159,255)),
        ('Bluetooth', 'Wireless', 'bluetooth', (184,120,255)),
        ('Display brightness', 'Display', 'brightness', (55,237,156)),
        ('Speaker volume', 'Audio', 'sound', (75,159,255))]


def focus_orb(complete):
    yy,xx=np.mgrid[0:44*S,0:44*S].astype(float)/S
    radius=np.hypot(xx-22,yy-22)
    halo=np.exp(-((radius/14)**4))*190
    pixels=np.zeros((44*S,44*S,4),dtype=np.uint8)
    pixels[:,:,:3]=(10,215,255)
    pixels[:,:,3]=halo.astype(np.uint8)
    inside=radius<=9.5
    shade=np.clip(1-radius/12,0,1)
    shine=np.exp(-(((xx-19)/4)**2+((yy-18)/3)**2))
    for channel,base in enumerate((25,190,235)):
        pixels[:,:,channel]=np.where(inside,np.clip(base+(255-base)*(shade*.55+shine*.85),0,255),pixels[:,:,channel])
    pixels[:,:,3]=np.where(inside,255,pixels[:,:,3])
    im=Image.fromarray(pixels)
    dr=ImageDraw.Draw(im)
    dr.ellipse((12.5*S,12.5*S,31.5*S,31.5*S),outline=(170,255,255),width=S)
    if complete: dr.line((16*S,22*S,20*S,26*S,28*S,18*S),fill=(0,63,84),width=2*S,joint='curve')
    return im


def row(index, focus):
    im = Image.new('RGBA', (364*S, 54*S))
    dr = ImageDraw.Draw(im)
    dr.rounded_rectangle((4*S,4*S,360*S,50*S), radius=9*S,
                         fill=(5,49,65,235) if focus else (5,19,28,235),
                         outline=(100,246,255,255) if focus else (28,72,96,255), width=S)
    cx, cy = 28, 27
    if focus:
        im.alpha_composite(focus_orb(False),((cx-22)*S,(cy-22)*S))
    else:
        dr.ellipse(((cx-8)*S,(cy-8)*S,(cx+8)*S,(cy+8)*S), outline=BLUE,width=S)
    title, label, key, color = ROWS[index]
    dr.text((50*S,9*S),title,font=ImageFont.truetype(str(FONT),14*S),fill=WHITE,anchor='lt')
    # Colored glass tag uses the same construction as the approved Tasks mockup.
    tag=Image.new('RGBA',(101*S,24*S))
    mask=Image.new('L',tag.size)
    ImageDraw.Draw(mask).rounded_rectangle((S,S,100*S,23*S),radius=12*S,fill=255)
    td=ImageDraw.Draw(tag)
    for py in range(24*S):
        f=0.34-0.15*py/(24*S-1)
        td.line((0,py,101*S,py),fill=tuple(round(c*f) for c in color)+(255,))
    tag.putalpha(mask)
    td.rounded_rectangle((S,S,100*S,23*S),radius=12*S,outline=color,width=S)
    ink=tuple(round(c*.45+255*.55) for c in color)
    icon=Image.new('L',(16*S,16*S))
    legacy.DRAW[key](ImageDraw.Draw(icon),lambda v:v*16*S,round(1.2*S))
    colored=Image.new('RGBA',icon.size,ink)
    colored.putalpha(icon)
    tag.alpha_composite(colored,(9*S,4*S))
    td.text((32*S,12*S),label,font=ImageFont.truetype(str(FONT),11*S),fill=ink,anchor='lm')
    im.alpha_composite(tag,(243*S,((54-24)//2)*S))
    return im.resize((364,54),Image.Resampling.LANCZOS)


def glow():
    im=Image.new('RGBA',(44*S,44*S))
    ImageDraw.Draw(im).ellipse((12*S,12*S,32*S,32*S),fill=(35,221,255,230))
    return im.filter(ImageFilter.GaussianBlur(5*S)).resize((44,44),Image.Resampling.LANCZOS)


def glyph(character):
    font=ImageFont.truetype(str(FONT),11*S)
    advance=round(font.getlength(character)*16/S)
    width=int(np.ceil(font.getlength(character)/S))+4
    im=Image.new('RGBA',(width*S,19*S))
    ImageDraw.Draw(im).text((2*S,12*S),character,font=font,fill=BLUE,anchor='ls')
    return im.resize((width,19),Image.Resampling.LANCZOS),advance


def main():
    install=argparse.ArgumentParser()
    install.add_argument('--install',action='store_true')
    args=install.parse_args()
    PREVIEW.mkdir(parents=True,exist_ok=True)
    assets=[]
    for i in range(4):
        for focused in (False,True):
            assets.append((f'row_{i}_{int(focused)}',row(i,focused),0))
    assets.append(('glow',glow(),0))
    frame=Image.new('RGBA',(364*S,54*S))
    ImageDraw.Draw(frame).rounded_rectangle((4*S,4*S,360*S,50*S),radius=9*S,outline=(130,255,255,255),width=S)
    assets.append(('focus_frame',frame.resize((364,54),Image.Resampling.LANCZOS),0))
    for code in range(33,127):
        im,advance=glyph(chr(code))
        assets.append((f'g_{code:02X}',im,advance))
    # All custom sprites must have clean transparent edges. ARGB8888 uses
    # no palette and requires no dithering or Designer conversion.
    for _,im,_ in assets:
        pixels=np.array(im)
        pixels[[0,-1],:]=0
        pixels[:,[0,-1]]=0
        im.paste(Image.fromarray(pixels))
        assert not np.asarray(im)[[0,-1],:,3].any()
        assert not np.asarray(im)[:,[0,-1],3].any()
    font=ImageFont.truetype(str(FONT),11*S)
    for value in ('HomeNet','On','Off','0%','100%','0/10','10/10'):
        assert font.getlength(value)/S<=180
    for title, label, _, _ in ROWS:
        assert ImageFont.truetype(str(FONT),14*S).getlength(title)/S<190
        assert font.getlength(label)/S<65
    canvas=Image.new('RGBA',(480,480),(0,0,0,255))
    base=ROOT/'TouchGFX/assets/images'
    board=Image.open(base/'board.png').convert('RGBA').transpose(Image.Transpose.ROTATE_270)
    board.putalpha(board.getchannel('A').point(lambda a:round(a*.4)))
    canvas.alpha_composite(board)
    dr=ImageDraw.Draw(canvas)
    dr.text((240,70),'SETTINGS',font=ImageFont.truetype(str(FONT),25),fill=WHITE,anchor='mt')
    dr.text((240,105),'CUSTOMIZE YOUR AURA',font=ImageFont.truetype(str(FONT),11),fill=BLUE,anchor='mt')
    for i,value in enumerate(('HomeNet','On','70%','5/10')):
        y=132+54*i
        if i==1: canvas.alpha_composite(glow(),(64,y+5))
        canvas.alpha_composite(row(i,i==1),(58,y))
        ImageDraw.Draw(canvas).text((108,y+31),value,font=ImageFont.truetype(str(FONT),11),fill=BLUE,anchor='lt')
    # Composite shared status header and perimeter from existing assets.
    header=Image.new('RGBA',(480,480))
    for name,position in [('aura/status/wifi_3.png',(31,283)),
                          ('aura/status/batt_frame.png',(35,176)),
                          ('aura/logo/logo_00.png',(20,217))]:
        header.alpha_composite(Image.open(base/name).convert('RGBA'),position)
    ImageDraw.Draw(header).rectangle((38,187,46,203),fill=(86,210,255))
    canvas.alpha_composite(header.transpose(Image.Transpose.ROTATE_270))
    canvas.alpha_composite(Image.open(base/'aura/rim/rim.png').convert('RGBA').transpose(Image.Transpose.ROTATE_270))
    for line,label in enumerate(('RIGHT SELECT','HOLD CLOSE TO GO BACK')):
        ImageDraw.Draw(canvas).text((240,400+15*line),label,font=ImageFont.truetype(str(FONT),11),fill=BLUE,anchor='mt')
    canvas.convert('RGB').save(PREVIEW/'settings.png')
    if not args.install: return
    OUT.mkdir(parents=True,exist_ok=True)
    header=['#pragma once\n#include <stdint.h>\n',
            'struct SettingsStyleSprite { const uint32_t* pixels; int16_t width, height; uint16_t advance16; };\n',
            'extern const SettingsStyleSprite settingsStyleRows[4][2];\n',
            'extern const SettingsStyleSprite settingsStyleGlow;\n',
            'extern const SettingsStyleSprite settingsStyleFocusFrame;\n',
            'extern const SettingsStyleSprite settingsStyleGlyphs[94];\n',
            f'constexpr uint16_t SETTINGS_STYLE_SPACE16 = {round(ImageFont.truetype(str(FONT),11*S).getlength(" ")*16/S)};\n']
    cpp=['#include "SettingsStyleAssets.hpp"\n#include <touchgfx/hal/Config.hpp>\n']
    entries={}
    for name,im,advance in assets:
        im.save(PREVIEW/(name+'.png'))
        # Existing PixelDataWidget paths use 180-degree source pixels, with
        # swapped widget dimensions, for the project's Portrait orientation.
        arr=np.asarray(im.transpose(Image.Transpose.ROTATE_180),dtype=np.uint32)
        words=((arr[:,:,3]<<24)|(arr[:,:,0]<<16)|(arr[:,:,1]<<8)|arr[:,:,2]).ravel()
        cpp.append(f'LOCATION_PRAGMA("ExtFlashSection")\nKEEP static const uint32_t {name}[] LOCATION_ATTRIBUTE("ExtFlashSection") = {{\n')
        cpp.extend('    '+','.join(f'0x{v:08x}' for v in words[j:j+12])+',\n' for j in range(0,len(words),12))
        cpp.append('};\n')
        entries[name]=f'{{ {name}, {im.height}, {im.width}, {advance} }}'
    cpp.append('const SettingsStyleSprite settingsStyleRows[4][2] = {\n'+
               ',\n'.join('    { '+entries[f'row_{i}_0']+', '+entries[f'row_{i}_1']+' }' for i in range(4))+'\n};\n')
    cpp.append('const SettingsStyleSprite settingsStyleGlow = '+entries['glow']+';\n')
    cpp.append('const SettingsStyleSprite settingsStyleFocusFrame = '+entries['focus_frame']+';\n')
    cpp.append('const SettingsStyleSprite settingsStyleGlyphs[94] = {\n'+
               ',\n'.join('    '+entries[f'g_{code:02X}'] for code in range(33,127))+'\n};\n')
    (OUT/'SettingsStyleAssets.hpp').write_text(''.join(header))
    (OUT/'SettingsStyleAssets.cpp').write_text(''.join(cpp))
    print(f'{len(assets)} custom sprites generated; QSPI pixel bytes: {sum(im.width*im.height*4 for _,im,_ in assets)}')


if __name__=='__main__': main()

# Keep the firmware bound to Designer bitmaps after regenerating artwork.
if __name__ == '__main__':
    import migrate_designer_assets
    migrate_designer_assets.main()
