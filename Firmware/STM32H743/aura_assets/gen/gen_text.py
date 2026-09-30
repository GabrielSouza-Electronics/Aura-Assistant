#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Rotulos de texto como SPRITES - Aura Assistant.

Mesma decisao dos digitos da bateria: um TextArea exigiria criar cada texto e
configurar a fonte no Designer, e dependencias de configuracao ja falharam em
silencio nesta integracao. Os textos aqui sao fixos e poucos, entao virar
imagem custa 20 KB e elimina o passo manual.

O tracking (letter-spacing) tambem so existe assim: o Designer nem sempre
expoe esse ajuste, e sem ele o texto em caixa alta fica apertado e perde a
leitura de painel tecnico.

Saida:
    text/lbl_<nome>.png    rotulo grande da opcao (centro da tela)
    text/ctx_<nome>.png    mesma palavra pequena, para sob a marca AURA
    text/msg_<nome>.png    frases de estado
"""

import os
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter

import os as _os
_HERE = _os.path.dirname(_os.path.abspath(__file__))
OUT = _os.path.join(_HERE, "text")

SS = 6
TRACK = 0.02            # fracao do corpo da fonte

def _font(name):
    # aura_assets/fonts/ primeiro (Windows); o caminho Linux original e o
    # fallback de quem gerou os assets anteriores.
    local = _os.path.join(_os.path.dirname(_HERE), "fonts", name)
    if _os.path.isfile(local):
        return local
    return "/usr/share/fonts/truetype/google-fonts/" + name


FONT_MED = _font("Poppins-Medium.ttf")
FONT_REG = _font("Poppins-Regular.ttf")

LABELS = ["SETTINGS", "TASKS", "REMINDERS", "CALENDAR", "CHAT"]
MSGS = {
    "hold": "HOLD TO SELECT",
    "cancelled": "CANCELLED",
    "move": "MOVE TO CONTINUE",
    "nocontent": "NO CONTENT YET",
    "back": "PULL DOWN TO GO BACK",
}

C_LOW, C_MID, C_HIGH = (30, 96, 150), (150, 215, 245), (235, 248, 255)


def render_text(s, pt, font_path, pad=6):
    f = ImageFont.truetype(font_path, pt * SS)
    probe = ImageDraw.Draw(Image.new("L", (8, 8)))
    track = TRACK * pt * SS
    w = int(sum(probe.textlength(c, font=f) + track for c in s) - track)
    h = int(pt * SS * 1.45)
    img = Image.new("L", (w + pad * 2 * SS, h), 0)
    dr = ImageDraw.Draw(img)
    x = pad * SS
    for c in s:
        dr.text((x, h / 2), c, font=f, fill=255, anchor="lm")
        x += probe.textlength(c, font=f) + track

    return finish(img)


def finish(img):
    """Calendar-style coverage: one downsample, constant color, clear edges."""
    alpha = np.asarray(img.resize((img.width // SS, img.height // SS),
                                  Image.Resampling.LANCZOS)).copy()
    alpha[[0, -1], :] = 0
    alpha[:, [0, -1]] = 0
    rgba = np.zeros((*alpha.shape, 4), np.uint8)
    rgba[:, :, :3] = C_HIGH
    rgba[:, :, 3] = alpha
    original = Image.fromarray(rgba)
    quantized = np.asarray(original.quantize(colors=255,
        method=Image.Quantize.FASTOCTREE, dither=Image.Dither.NONE)
        .convert("RGBA")).copy()
    quantized[alpha == 0] = 0
    return Image.fromarray(quantized)


def main():
    os.makedirs(OUT, exist_ok=True)
    os.makedirs(_os.path.join(_HERE, "preview"), exist_ok=True)
    total = 0
    lines = []

    for s in LABELS:
        im = render_text(s, 24, FONT_MED)
        im.save(f"{OUT}/lbl_{s.lower()}.png")
        total += im.width * im.height + 1024
        lines.append(f"    {{ BITMAP_LBL_{s}_ID, {im.width}, {im.height} }},")

    for s in LABELS:
        im = render_text(s, 12, FONT_MED)
        im.save(f"{OUT}/ctx_{s.lower()}.png")
        total += im.width * im.height + 1024

    for key, s in MSGS.items():
        im = render_text(s, 14, FONT_MED)
        im.save(f"{OUT}/msg_{key}.png")
        total += im.width * im.height + 1024

    render_text("READY", 30, FONT_MED).save(f"{OUT}/sb_ready.png")
    render_text("WAVE TO BEGIN", 14, FONT_MED).save(f"{OUT}/sb_wave.png")

    print(f"  {len(LABELS)*2 + len(MSGS)} sprites de texto")
    print(f"  flash: {total/1024:.0f} KB em L8_ARGB8888")

    # tabela pronta para colar no OrbitalMenu.cpp
    print("\n  --- dimensoes (para o .cpp) ---")
    for s in LABELS:
        a = Image.open(f"{OUT}/lbl_{s.lower()}.png")
        b = Image.open(f"{OUT}/ctx_{s.lower()}.png")
        print(f"    {s:10s} lbl {a.width}x{a.height}   ctx {b.width}x{b.height}")
    for key in MSGS:
        m = Image.open(f"{OUT}/msg_{key}.png")
        print(f"    msg_{key:10s} {m.width}x{m.height}")

    sh = Image.new("RGBA", (300, 30 * (len(LABELS) + len(MSGS)) + 20), (3, 10, 26, 255))
    y = 8
    for s in LABELS:
        im = Image.open(f"{OUT}/lbl_{s.lower()}.png")
        sh.alpha_composite(im, (10, y))
        y += 30
    for key in MSGS:
        im = Image.open(f"{OUT}/msg_{key}.png")
        sh.alpha_composite(im, (10, y))
        y += 30
    sh.convert("RGB").resize((600, sh.height * 2), Image.LANCZOS).save(
        _os.path.join(_HERE, "preview") + "/text.png")


if __name__ == "__main__":
    main()
