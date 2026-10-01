"""Review-only calendar navigation mockup; does not install runtime assets."""
from pathlib import Path
import sys
from PIL import Image, ImageDraw, ImageFont

BASE = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BASE / 'gen'))
import gen_calendar as cal


def frame(assets, direction, month=10):
    images = dict(assets)
    for name, sign in [('left', -1), ('right', 1)]:
        arrow = assets['cal_arrow_' + name]
        tile = Image.new('RGBA', (44, 44))
        if direction == sign:
            tile.alpha_composite(assets['cal_today'])
        cal.center(tile, arrow, 22, 22, 255 if direction == sign else 150)
        images['cal_arrow_' + name] = tile
    return cal.compose(images, 2026, month, 0, set(), leak=True)


def main():
    assets = {p.stem: Image.open(p).convert('RGBA')
              for p in (BASE / 'gen/calendar').glob('*.png')}
    sheet = Image.new('RGB', (1440, 520), '#101923')
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.truetype(str(cal.FONT), 17)
    for i, (direction, label) in enumerate([(0, 'NEUTRO'), (-1, 'ESQUERDA'), (1, 'DIREITA')]):
        sheet.paste(frame(assets, direction), (i * 480, 40))
        draw.text((i * 480 + 180, 10), label, font=font, fill='white')
    sheet.save(BASE / 'preview/calendar_navigation.png')
    frames = [frame(assets, 0)]
    frames += [frame(assets, 1, month) for month in [10, 11, 12]]
    frames += [frame(assets, 0, 12)]
    frames += [frame(assets, -1, month) for month in [12, 11, 10]]
    frames += [frame(assets, 0)]
    frames[0].save(BASE / 'preview/calendar_navigation.gif', save_all=True,
                   append_images=frames[1:], duration=650, loop=0)


if __name__ == '__main__':
    main()
