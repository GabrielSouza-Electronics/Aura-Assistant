"""Preview sharper carousel icons without changing installed TouchGFX assets."""
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import gen_icons as source

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "preview" / "menu_icons_sharp"


def sharp_icon(name, size):
    scale = 8
    mask = Image.new("L", (size * scale, size * scale))
    source.DRAW[name](ImageDraw.Draw(mask), lambda v: v * size * scale,
                      round(0.044 * size * scale))
    mask = mask.resize((size, size), Image.Resampling.LANCZOS)
    core = np.asarray(mask, dtype=np.float32) / 255
    # A bright core and a compact halo retain the neon look without washing
    # out the gaps between strokes as the original broad bloom did.
    halo = np.asarray(mask.filter(ImageFilter.GaussianBlur(0.85)), dtype=np.float32) / 255
    bloom = np.asarray(mask.filter(ImageFilter.GaussianBlur(3.0)), dtype=np.float32) / 255
    # Keep bloom out of the dim internal details: these need contrast to
    # remain readable, especially in the smallest carousel size.
    detail = (core > 0.15) & (core < 0.65)
    glow = halo * 0.60 + bloom * 0.65
    glow = np.where(detail, glow * 0.65, glow)
    result = source.colorize(np.clip(core * 1.18 + glow, 0, 1), levels=190)
    pixels = np.array(result)
    pixels[[0, -1], :, :] = 0
    pixels[:, [0, -1], :] = 0
    pixels[pixels[:, :, 3] == 0] = 0
    result = Image.fromarray(pixels)
    assert len(result.getcolors(size * size)) <= 256
    return result


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    sheet = Image.new("RGB", (900, 700), (5, 13, 26))
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.truetype("C:/Windows/Fonts/segoeui.ttf", 18)
    draw.text((20, 12), "ANTES / BRILHO + NITIDEZ  |  tamanhos reais + detalhe ampliado 1,5x", font=font, fill="white")
    labels = ["Ajustes", "Tarefas", "Lembretes", "Calendario", "Chat"]
    for column, name in enumerate(source.NAMES):
        x = column * 178 + 15
        draw.text((x, 48), labels[column], font=font, fill=(100, 210, 255))
        for row, size in enumerate(source.SIZES):
            old = Image.open(ROOT.parent / "TouchGFX/assets/images/aura/icons" / f"{name}_{size}.png").convert("RGBA").transpose(Image.Transpose.ROTATE_270)
            new = sharp_icon(name, size)
            new.save(OUT / f"{name}_{size}.png")
            y = 82 + row * 86
            draw.text((x, y), str(size), font=font, fill=(140, 150, 170))
            for offset, icon in ((30, old), (105, new)):
                sheet.paste(icon, (x + offset, y + (64 - size) // 2), icon)
        for row, (label, icon) in enumerate((("Antes", old), ("Proposta", new))):
            y = 355 + row * 112
            draw.text((x, y), label, font=font, fill="white")
            # Nearest-neighbour enlargement exposes actual pixel edges.
            detail = icon.resize((96, 96), Image.Resampling.NEAREST)
            sheet.paste(detail, (x + 68, y), detail)
        draw.rectangle((x, 585, x + 164, 690), fill=(35, 43, 56))
        draw.text((x + 5, 588), "Fundo clareado", font=font, fill="white")
        sheet.paste(new, (x + 50, 620), new)
    sheet.save(OUT.parent / "menu_icons_comparison.png")
    print("15 preview icons validated: <=256 RGBA colors, transparent edges.")
    print(OUT.parent / "menu_icons_comparison.png")


if __name__ == "__main__":
    main()
