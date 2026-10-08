"""Generate approved Tasks sprites and validate deadline text widths."""
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import preview_tasks as design
import gen_settings_tasks_style as style
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'aura_assets/compiled/tasks'
S=6

def focus_orb(complete):
    return style.focus_orb(complete)

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    assets=[]
    def add(name,im,advance=0):
        im=im.resize((im.width//S,im.height//S),Image.Resampling.LANCZOS)
        arr=np.array(im); arr[[0,-1],:]=0; arr[:,[0,-1]]=0
        im=Image.fromarray(arr)
        assets.append((name,im,advance))
    panel=Image.new('RGBA',(350*S,57*S))
    ImageDraw.Draw(panel).rounded_rectangle((S,S,349*S,56*S),radius=10*S,
        fill=(5,19,28,235),outline=(28,72,96),width=S)
    add('panel',panel)
    for category in ('Personal','Work','Priority','Project'):
        add(category.lower(),design.badge(category))
    for name,selected,complete in [('circle',False,False),('focus',True,False),('done',False,True),('focus_done',True,True)]:
        if selected:
            add(name,focus_orb(complete))
            continue
        im=Image.new('RGBA',(44*S,44*S)); dr=ImageDraw.Draw(im)
        if selected:
            dr.ellipse((12*S,12*S,32*S,32*S),fill=(35,221,255,210))
            im=im.filter(ImageFilter.GaussianBlur(5*S)); dr=ImageDraw.Draw(im)
        dr.ellipse((13*S,13*S,31*S,31*S),fill=(100,246,255) if selected or complete else None,
                   outline=(205,255,255) if selected else (156,210,234),width=S)
        if complete: dr.line((16*S,22*S,20*S,26*S,28*S,18*S),fill=(0,60,83),width=2*S,joint='curve')
        add(name,im)
    clock=Image.new('RGBA',(20*S,20*S)); design.icon(ImageDraw.Draw(clock),'Clock',2,2,(160,205,238)); add('clock',clock)
    for name,sign in [('left',-1),('right',1)]:
        im=Image.new('RGBA',(28*S,28*S)); dr=ImageDraw.Draw(im)
        points=[((14+sign*3)*S,6*S),((14-sign*4)*S,14*S),((14+sign*3)*S,22*S)]
        # The tip points left for sign=-1, right for sign=+1.
        points=[((14-sign*3)*S,6*S),((14+sign*4)*S,14*S),((14-sign*3)*S,22*S)]
        dr.line(points,fill=(160,220,255),width=2*S,joint='curve'); add(name,im)
    add('arrow_glow',style.glow().resize((44*S,44*S),Image.Resampling.LANCZOS))
    highlight=Image.new('RGBA',panel.size)
    hd=ImageDraw.Draw(highlight)
    hd.rounded_rectangle((2*S,2*S,348*S,55*S),radius=10*S,outline=(15,221,255,210),width=3*S)
    highlight=highlight.filter(ImageFilter.GaussianBlur(2*S))
    hd=ImageDraw.Draw(highlight)
    hd.rounded_rectangle((S,S,349*S,56*S),radius=10*S,fill=(5,49,65,200),outline=(100,246,255,255),width=S)
    add('panel_focus',highlight)
    halo=Image.new('RGBA',(44*S,44*S))
    ImageDraw.Draw(halo).ellipse((8*S,8*S,36*S,36*S),fill=(0,218,255,180))
    add('focus_halo',halo.filter(ImageFilter.GaussianBlur(5*S)))
    import sys
    reminder_preview='--reminders-preview' in sys.argv
    preview=ROOT/('aura_assets/preview/reminders_menu' if reminder_preview else 'aura_assets/preview/tasks_focus')
    preview.mkdir(parents=True,exist_ok=True)
    frames=[]
    for frame in range(24):
        canvas=Image.new('RGBA',(480,480),(2,9,14,255))
        d=ImageDraw.Draw(canvas)
        d.text((240,55),'REMINDERS' if reminder_preview else 'TASKS',font=ImageFont.truetype(str(style.FONT),25),fill=style.WHITE,anchor='mt')
        d.ellipse((320,52,388,120),outline=(23,44,58),width=3)
        d.arc((320,52,388,120),-90,0,fill=(48,231,255),width=3)
        d.text((354,70),'2/8',font=ImageFont.truetype(str(style.FONT),11),fill=style.WHITE,anchor='mt')
        d.text((354,88),'completed',font=ImageFont.truetype(str(style.FONT),11),fill=style.BLUE,anchor='mt')
        pulse=round(220+35*np.sin(frame*2*np.pi/24))
        for i,(title,category) in enumerate([('Check project milestone','Project'),('Call the team','Work'),('Send PCB for review','Priority'),('Take a walking break','Personal')] if reminder_preview else [('Prepare project report','Project'),('Team meeting','Work'),('Review PCB design','Priority'),('Plan training session','Personal')]):
            x,y=65,133+63*i
            canvas.alpha_composite(assets[0][1],(x,y))
            selected=i==1
            if selected:
                overlay=assets[13][1].copy(); overlay.putalpha(overlay.getchannel('A').point(lambda a:round(a*pulse/255)))
                canvas.alpha_composite(overlay,(x,y))
                canvas.alpha_composite(assets[14][1],(68,y+7))
            canvas.alpha_composite(assets[6 if selected else 5][1],(68,y+7))
            d=ImageDraw.Draw(canvas)
            d.text((110,y+8),title,font=ImageFont.truetype(str(style.FONT),14),fill=style.WHITE)
            canvas.alpha_composite(assets[9][1],(110,y+30))
            d.text((133,y+32),f'{9+i:02}:00',font=ImageFont.truetype(str(style.FONT),11),fill=style.BLUE)
            canvas.alpha_composite(assets[1+('Personal','Work','Priority','Project').index(category)][1],(303,y+16))
        frames.append(canvas.convert('RGB'))
    frames[0].save(preview/'selection.png')
    frames[0].save(preview/'selection.gif',save_all=True,append_images=frames[1:],duration=50,loop=0)
    ink_bounds=[]
    for code in range(33,127):
        font=ImageFont.truetype(str(style.FONT),14*S)
        advance=round(font.getlength(chr(code))*16/S)
        width=int(np.ceil(font.getlength(chr(code))/S))+4
        im=Image.new('RGBA',(width*S,23*S))
        ImageDraw.Draw(im).text((2*S,15*S),chr(code),font=font,fill=(235,248,255),anchor='ls')
        bounds=font.getbbox(chr(code),anchor='ls')
        ink_bounds.append((round(15+bounds[1]/S),round(15+bounds[3]/S)))
        add(f'title_{code:02X}',im,advance)
    for size in (11,14):
        font=ImageFont.truetype(str(style.FONT),size*S)
        im=Image.new('RGBA',(9*S,23*S if size==14 else 19*S))
        ImageDraw.Draw(im).text((2*S,(15 if size==14 else 12)*S),'\u2022',font=font,fill=(160,205,238),anchor='ls')
        add(f'bullet_{size}',im,round(font.getlength('\u2022')*16/S))
    font=ImageFont.truetype(str(style.FONT),11*S)
    longest=max((f'{word} \u2022 {h:02}:{m:02}' for word in ('Yesterday','Tomorrow') for h in range(24) for m in range(60)),key=lambda t:font.getlength(t))
    assert font.getlength(longest)/S<165
    h=['#pragma once\n#include "SettingsStyleAssets.hpp"\n',
       'enum TaskSpriteId { TASK_PANEL,TASK_PERSONAL_TAG,TASK_WORK_TAG,TASK_PRIORITY_TAG,TASK_PROJECT_TAG,\n',
       'TASK_CIRCLE,TASK_FOCUS,TASK_DONE,TASK_FOCUS_DONE,TASK_CLOCK,TASK_LEFT,TASK_RIGHT,TASK_ARROW_GLOW,TASK_PANEL_FOCUS,TASK_FOCUS_HALO };\n',
       'extern const SettingsStyleSprite taskSprites[15];\n',
       'extern const SettingsStyleSprite taskTitleGlyphs[94];\n',
       'extern const int8_t taskTitleInk[94][2];\n',
       'extern const SettingsStyleSprite taskBullets[2];\n',
       f'constexpr uint16_t TASK_TITLE_SPACE16 = {round(ImageFont.truetype(str(style.FONT),14*S).getlength(" ")*16/S)};\n']
    cpp=['#include "TasksAssets.hpp"\n#include <touchgfx/hal/Config.hpp>\n']; entries=[]
    for name,im,advance in assets:
        arr=np.asarray(im.transpose(Image.Transpose.ROTATE_180),dtype=np.uint32)
        words=((arr[:,:,3]<<24)|(arr[:,:,0]<<16)|(arr[:,:,1]<<8)|arr[:,:,2]).ravel()
        cpp.append(f'LOCATION_PRAGMA("ExtFlashSection")\nKEEP static const uint32_t {name}[] LOCATION_ATTRIBUTE("ExtFlashSection") = {{\n')
        cpp.extend('    '+','.join(f'0x{v:08x}' for v in words[i:i+12])+',\n' for i in range(0,len(words),12)); cpp.append('};\n')
        entries.append(f'    {{ {name}, {im.height}, {im.width}, {advance} }}')
    for name,n,part in [('taskSprites',15,entries[:15]),('taskTitleGlyphs',94,entries[15:109]),('taskBullets',2,entries[109:])]:
        assert len(part)==n
        cpp.append(f'const SettingsStyleSprite {name}[{n}] = {{\n'+',\n'.join(part)+'\n};\n')
    (OUT/'TasksAssets.hpp').write_text(''.join(h))
    cpp.append('const int8_t taskTitleInk[94][2] = {\n'+',\n'.join(f'    {{ {top}, {bottom} }}' for top,bottom in ink_bounds)+'\n};\n')
    (OUT/'TasksAssets.cpp').write_text(''.join(cpp))
    print(f'{len(assets)} Tasks sprites generated; deadline width max {font.getlength(longest)/S:.1f}px / 165px')
if __name__=='__main__': main()

# Keep the firmware bound to Designer bitmaps after regenerating artwork.
if __name__ == '__main__':
    import migrate_designer_assets
    migrate_designer_assets.main()
