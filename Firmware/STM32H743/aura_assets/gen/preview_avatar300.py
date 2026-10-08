"""Review-only 300px avatar previews. Inputs are already pre-rotated left."""
import argparse
import json
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
GROUPS = {'Idle':6, 'blink':6, 'LookUp':12, 'Lookdown':5,
          'Lookright':8, 'Lookleft':9, 'Thinking':19,
          'Goodbye':22, 'Coffee':26, 'Smile':11}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    args = parser.parse_args()
    out = ROOT / 'preview/avatar300'
    out.mkdir(parents=True, exist_ok=True)
    assets = ROOT.parent / 'TouchGFX/assets/images'
    base = Image.new('RGBA', (480,480), (0,0,0,255))
    base.alpha_composite(Image.open(assets/'board.png').convert('RGBA'))
    title = Image.open(assets/'aura/text/lbl_chat.png').convert('RGBA')
    base.alpha_composite(title, (74-title.width//2,239-title.height//2))
    base = base.transpose(Image.Transpose.ROTATE_270)
    mask = Image.new('L',(480,480))
    ImageDraw.Draw(mask).ellipse((0,0,479,479), fill=255)

    def render(path):
        sprite = Image.open(path).convert('RGBA')
        assert sprite.size == (300,300), path
        assert sprite.getchannel('A').getextrema() == (0,255), path
        assert sprite.getcolors(256) is not None, path
        # Undo source rotation only for physical-view preview, never the source.
        canvas = base.copy()
        canvas.alpha_composite(sprite.transpose(Image.Transpose.ROTATE_270),(90,90))
        image = Image.new('RGBA',(480,480),(18,18,18,255))
        image.paste(canvas,(0,0),mask)
        return image.convert('RGB')

    rendered = {}
    orders = {}
    sheet = Image.new('RGB',(480*5,510*2),(18,18,18))
    for index,(name,count) in enumerate(GROUPS.items()):
        images = [render(args.source/f'{name}{i}.png') for i in range(1,count+1)]
        order = list(range(count)) if name=='blink' else list(range(count))+list(range(count-2,-1,-1))
        sequence = [images[i] for i in order]
        rendered[name] = sequence
        orders[name] = [i+1 for i in order]
        sequence[0].save(out/f'{name}.gif',save_all=True,append_images=sequence[1:],duration=100,loop=0,disposal=2)
        cell_x,cell_y = (index%5)*480,(index//5)*510
        sheet.paste(images[count//2],(cell_x,cell_y))
        ImageDraw.Draw(sheet).text((cell_x+12,cell_y+484),f'{name}: {count} frames / 10 fps',fill=(230,240,255))
    sheet.save(out/'overview.png')
    rendered['Idle'][0].save(out/'centered.png')
    prep = sum([rendered[name] for name in ('Thinking','Idle','Smile','blink','Idle')],[])
    prep[0].save(out/'preparation.gif',save_all=True,append_images=prep[1:],duration=100,loop=0,disposal=2)
    (out/'manifest.json').write_text(json.dumps({'fps':10,'size':[300,300],
        'framebuffer_xy':[90,90],'frames':GROUPS,'orders':orders,
        'estimated_l8_bytes_upper_bound':sum(GROUPS.values())*(90000+1028),
        'note':'Smile preview uses all 11 supplied frames pending clarification. No firmware changes.'},indent=2)+'\n')
    print(f'{sum(GROUPS.values())} transparent 300x300 frames validated; previews: {out}')


if __name__=='__main__':
    main()
