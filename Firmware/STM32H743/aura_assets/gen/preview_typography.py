"""Review-only typography/assets proposal; never installs or edits GUI files.

Reuse the current panel/icon design, with the calendar's 6x native-size
antialiasing. All output stays in aura_assets/preview/typography/.
"""
from pathlib import Path
import json
import math
import numpy as np
from PIL import Image, ImageDraw, ImageFont
import gen_text as gt
import gen_settings as gs
import gen_icons as gi
import gen_status as status

BASE = Path(__file__).resolve().parents[1]
OUT = BASE/'preview/typography'
SOURCE = OUT/'assets'
SCALE = 6
WHITE = (235,248,255)


def finish(mask):
    alpha=np.asarray(mask.resize((mask.width//SCALE,mask.height//SCALE),Image.Resampling.LANCZOS)).copy()
    alpha[[0,-1],:]=0
    alpha[:,[0,-1]]=0
    rgba=np.zeros((*alpha.shape,4),np.uint8)
    rgba[:,:,:3]=gt.C_HIGH
    rgba[:,:,3]=alpha
    return gs.quant256(Image.fromarray(rgba),preserve_clear=True)


def text(label,pt,color=WHITE):
    saved=gt.C_HIGH
    gt.C_HIGH=color
    try:
        return original_text(label,pt,gt.FONT_MED)
    finally:
        gt.C_HIGH=saved


def enlarged(label,pt,font,pad=6):
    return original_text(label,pt,gt.FONT_MED,pad)


def centered(canvas,sprite,x,y):
    canvas.alpha_composite(sprite,(round(x-sprite.width/2),round(y-sprite.height/2)))


def fit_value(glyphs,space,value,width):
    result=gs.glyph_string(glyphs,space,value)
    while result.width>width+2*gs.GLYPH_PAD:
        value=value[:-1]
        result=gs.glyph_string(glyphs,space,value+'..')
    return result


def settings_preview(assets,glyphs,space,values,leak=False):
    im=gs.make_background(0)
    if leak:
        im=Image.blend(im,Image.new('RGBA',im.size,(60,60,60,255)),0.12)
    centered(im,text('SETTINGS',24),240,gs.TITLE_CY)
    centered(im,assets['sub_title'][0],240,gs.SUB_CY)
    for i,(_,label) in enumerate(gs.ROWS):
        if i==2:
            glow,(x,y)=assets[f'glow_{i}']
            im.alpha_composite(gs.fade(glow,0.85),(x,y))
        panel,(x,y)=assets[f'rowf_{i}' if i==2 else f'row_{i}']
        im.alpha_composite(panel,(x,y))
        max_width=gs.VALUE_RX-(gs.LABEL_X+text(label,18).width-12)-14
        value=fit_value(glyphs,space,values[i],max_width)
        cy=gs.ROW_Y0+i*gs.ROW_PITCH+gs.ROW_H/2
        im.alpha_composite(value,(round(gs.VALUE_RX+gs.GLYPH_PAD-value.width),round(cy-value.height/2)))
    centered(im,text('RIGHT SELECT / HOLD CLOSE 2s TO GO BACK',14,(184,223,243)),240,418)
    return im.convert('RGB')


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    SOURCE.mkdir(parents=True,exist_ok=True)
    assets=gs.build_all()
    glyphs,space=gs.build_glyphs()
    manifest={}
    def save(name,im):
        im=gs.quant256(im,preserve_clear=True)
        assert gs.ncolors(im)<=256,name
        assert gs.edge_alpha(im)==0,name
        im.save(SOURCE/(name+'.png'))
        manifest[name]={'width':im.width,'height':im.height,'colors':gs.ncolors(im)}
    for name,(im,_) in assets.items(): save('settings_'+name,im)
    for code,(im,_) in glyphs.items(): save(f'g_{code:02X}',im)
    labels={**{f'lbl_{s.lower()}':(s,24) for s in gt.LABELS},
            **{f'ctx_{s.lower()}':(s,12) for s in gt.LABELS},
            **{f'msg_{k}':(v,14) for k,v in gt.MSGS.items()},
            'sb_ready':('READY',30),'sb_wave':('WAVE TO BEGIN',14)}
    for name,(label,size) in labels.items(): save(name,text(label,size))
    for name in gi.NAMES:
        for size in gi.SIZES:
            im=gi.build_icon(name,size)
            # Explicit clear boundary preserves the existing sprite contract.
            a=np.asarray(im).copy();a[[0,-1],:]=0;a[:,[0,-1]]=0
            save(f'{name}_{size}',Image.fromarray(a))
    for level in range(4):
        a=np.asarray(status.build_wifi(level)).copy()
        a[[0,-1],:]=0; a[:,[0,-1]]=0
        save(f'wifi_{level}',Image.fromarray(a))
    for name,im in [('batt_frame',status.build_batt_frame()),('batt_bolt',status.build_bolt())]:
        a=np.asarray(im).copy();a[[0,-1],:]=0;a[:,[0,-1]]=0
        save(name,Image.fromarray(a))
    settings_preview(assets,glyphs,space,['AURA WIFI','ON','100%','10/10']).save(OUT/'settings.png')
    settings_preview(assets,glyphs,space,['AURA WIFI','ON','100%','10/10'],True).save(OUT/'settings_leak.png')
    settings_preview(assets,glyphs,space,['AURA-VERY-LONG-NETWORK-NAME-1234567','OFF','10%','0/10']).save(OUT/'settings_long_ssid.png')
    sheet=Image.new('RGBA',(720,560),(3,10,20,255))
    centered(sheet,text('CURRENT',16),180,24)
    centered(sheet,text('PROPOSED',16),540,24)
    originals=BASE.parent/'TouchGFX/assets/images/aura/text'
    for row,name in enumerate(['lbl_settings','lbl_reminders','msg_hold','msg_back','sb_ready','sb_wave']):
        old=Image.open(originals/(name+'.png')).convert('RGBA').rotate(-90,expand=True)
        new=Image.open(SOURCE/(name+'.png')).convert('RGBA')
        centered(sheet,old,180,85+row*55)
        centered(sheet,new,540,85+row*55)
    for i,name in enumerate(gi.NAMES):
        centered(sheet,Image.open(SOURCE/(name+'_48.png')).convert('RGBA'),120+i*120,457)
    centered(sheet,text('6X RENDER - NATIVE 480 X 480',14),360,522)
    sheet.convert('RGB').save(OUT/'comparison.png')
    manifest['glyph_max_advance16']=max(round(a*16) for _,a in glyphs.values())
    manifest['sizes']={'settings_label':18,'settings_value':18,'title':24,'subtitle':12,'hint':14}
    (OUT/'manifest.json').write_text(json.dumps(manifest,indent=2))
    print(f'{len(manifest)-2} review assets validated; max advance16={manifest["glyph_max_advance16"]}. No installed assets changed.')


original_text=gt.render_text
gt.SS=gs.SS=gi.SS=status.SS=SCALE
gt.TRACK=0.02
gt.FONT_REG=gt.FONT_MED
gt.C_HIGH=WHITE
gt.finish=finish
gt.render_text=enlarged
gs.GLYPH_PT=18
gs.ICON_N=30
gs.VAL_RAMP=((184,223,243),)*3

if __name__=='__main__':
    main()
