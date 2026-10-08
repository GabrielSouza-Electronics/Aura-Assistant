"""Review-only procedural light arc and existing circuit particle sprites."""
from pathlib import Path
import math
from PIL import Image, ImageDraw, ImageFilter

ROOT=Path(__file__).resolve().parents[1]
ASSETS=ROOT.parent/'TouchGFX/assets/images'
OUT=ROOT/'preview/chat_glow'
OUT.mkdir(parents=True,exist_ok=True)
base=Image.new('RGBA',(480,480),(0,0,0,255))
base.alpha_composite(Image.open(ASSETS/'board.png').convert('RGBA'))
title=Image.open(ASSETS/'aura/text/lbl_chat.png').convert('RGBA')
base.alpha_composite(title,(74-title.width//2,239-title.height//2))
base=base.transpose(Image.Transpose.ROTATE_270)
avatar=Image.open(ASSETS/'aura/avatar300/Idle/Idle1.png').convert('RGBA').transpose(Image.Transpose.ROTATE_270)
pulses=[Image.open(ASSETS/f'aura/circuit/pulse_11_{i}.png').convert('RGBA').transpose(Image.Transpose.ROTATE_270) for i in range(8)]
rings=[Image.open(ASSETS/f'aura/circuit/ringed_20_{i}.png').convert('RGBA').transpose(Image.Transpose.ROTATE_270) for i in range(8)]
divider=Image.open(ASSETS/'aura/divider/line.png').convert('RGBA').transpose(Image.Transpose.ROTATE_270)
divider=divider.resize((320,divider.height*2),Image.Resampling.LANCZOS)
sparks=[Image.open(ASSETS/f'aura/divider/spark_{i:02d}.png').convert('RGBA').transpose(Image.Transpose.ROTATE_270) for i in range(14)]
mask=Image.new('L',(480,480))
ImageDraw.Draw(mask).ellipse((0,0,479,479),fill=255)

def render(frame,active=False,leak=False):
    canvas=base.copy()
    if leak:
        canvas=Image.blend(canvas,Image.new('RGBA',canvas.size,(30,35,40,255)),0.45)
    canvas.alpha_composite(avatar,(90,90))
    t=frame*0.1
    strength=0.72+0.20*math.sin(t*2.8)+(0.08*math.sin(t*12) if active else 0)
    # Enlarge the actual divider artwork; preserve its line and glow design.
    light=divider.copy()
    light.putalpha(light.getchannel('A').point(lambda a:round(a*strength)))
    canvas.alpha_composite(light,(80,390-light.height//2))
    spark=sparks[frame%14].resize((sparks[0].width*2,sparks[0].height*2),Image.Resampling.LANCZOS)
    canvas.alpha_composite(spark,(round(240+125*math.sin(t*1.7)-spark.width/2),390-spark.height//2))
    # Surround the avatar on a full elliptical orbit, reusing existing sprites.
    for i in range(26):
        angle=i*math.tau/26+t*(1.3 if active else 0.55)
        x=240+167*math.cos(angle)
        y=249+151*math.sin(angle)+(3*math.sin(t*8+i) if active else 0)
        sprite=(rings if i%5==0 else pulses)[(frame+i)%8].copy()
        sprite.putalpha(sprite.getchannel('A').point(lambda a:round(a*(0.85 if active else 0.62))))
        canvas.alpha_composite(sprite,(round(x-sprite.width/2),round(y-sprite.height/2)))
    result=Image.new('RGBA',(480,480),(18,18,18,255))
    result.paste(canvas,(0,0),mask)
    return result.convert('RGB')

frames=[render(i,i>=24) for i in range(48)]
frames[0].save(OUT/'chat_glow.gif',save_all=True,append_images=frames[1:],duration=100,loop=0,disposal=2)
render(8).save(OUT/'chat_glow.png')
render(8,leak=True).save(OUT/'chat_glow_leak.png')
print(OUT)
