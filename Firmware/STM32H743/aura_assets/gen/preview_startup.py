"""Startup text proposal only; does not modify installed assets or firmware."""
from pathlib import Path
import math
from PIL import Image
import gen_text as gt

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT/'aura_assets/preview'

def center(canvas, sprite, x, y):
    canvas.alpha_composite(sprite, (round(x-sprite.width/2), round(y-sprite.height/2)))

def main():
    frames=[]
    base=ROOT/'TouchGFX/assets/images'
    credit=gt.render_text('DEVELOPED BY:', 11, gt.FONT_MED)
    developer=gt.render_text('GABRIEL SOUZA', 16, gt.FONT_MED)
    label=gt.render_text('STARTING, PLEASE WAIT', 14, gt.FONT_MED)
    dot=gt.render_text('.', 14, gt.FONT_MED)
    heroes=[Image.open(next(base.rglob(f'hero_{i:02}.png'))).convert('RGBA').rotate(-90,expand=True) for i in range(24)]
    rim=Image.open(next(base.rglob('rim.png'))).convert('RGBA').rotate(-90,expand=True)
    divider=Image.open(base/'aura/divider/line.png').convert('RGBA').rotate(-90,expand=True)
    sparks=[Image.open(base/f'aura/divider/spark_{i:02}.png').convert('RGBA').rotate(-90,expand=True) for i in range(14)]
    for frame in range(48):
        canvas=Image.new('RGBA',(480,480),(0,0,0,255))
        center(canvas,rim,240,240)
        center(canvas,heroes[frame%24],240,240)
        center(canvas,label,231,347)
        for i in range((frame//6)%4):
            center(canvas,dot,231+label.width/2+2+i*5,347)
        center(canvas,divider,240,370)
        center(canvas,sparks[frame%14],240+110*math.sin(2*math.pi*frame/48),370)
        center(canvas,credit,240,388)
        center(canvas,developer,240,409)
        frames.append(canvas.convert('RGB'))
    OUT.mkdir(parents=True,exist_ok=True)
    frames[18].save(OUT/'startup_text.png')
    frames[0].save(OUT/'startup_text.gif',save_all=True,append_images=frames[1:],duration=125,loop=0)

if __name__=='__main__':
    main()
