#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Logo do Aura a partir do SVG - Aura Assistant.

O logo RESPIRA: brilho e tamanho variam juntos, como um objeto vivo. Como o
TextureMapper depende de uma feature opcional do framework (ja nos custou o
nucleo invisivel), a escala vem de sprites pre-renderizados em varios
tamanhos - o codigo escolhe o mais proximo, igual ao nucleo dourado.

    N_SIZES tamanhos x N_GLOW fases de brilho

O SVG e recolorido para BRANCO e ganha um glow azul por tras. O desenho
original e azul chapado; branco com halo azul le melhor sobre o fundo escuro
e combina com a paleta do resto da interface.

Saida: logo/logo_<tamanho>_<fase>.png
"""

import os, math
import numpy as np
from PIL import Image, ImageFilter

import os as _os
_HERE = _os.path.dirname(_os.path.abspath(__file__))
OUT = _os.path.join(_HERE, "logo")

SVG = "/mnt/user-data/uploads/gabriel_alves_icon_blender_ready.svg"
ARC = "/mnt/user-data/uploads/LogoArc.png"

# Arco sob o logo. Respira na MESMA fase, com os mesmos N_FRAMES: se cada um
# tivesse seu ciclo, os dois se descasariam e o conjunto pareceria dois
# elementos separados em vez de um so.
ARC_W = 34             # largura final
ARC_CANVAS_H = 16      # altura do canvas, com folga para o glow

# CANVAS FIXO com o desenho variando dentro dele. Antes eram sprites de
# tamanhos diferentes e o codigo escolhia o mais proximo - com 5 tamanhos
# entre 48 e 55 px o passo era de 2 px, visivel como salto. Agora todos os
# frames tem o mesmo canvas e o desenho cresce por dentro, entao da para ter
# muitos passos e o antialiasing suaviza o resto.
CANVAS = 44            # tamanho de todos os sprites
BASE = 26              # desenho no menor frame
SCALE_MIN, SCALE_MAX = 1.00, 1.07   # amplitude discreta: o logo respira
                                    # sem mudar de tamanho de forma obvia
N_FRAMES = 20          # um ciclo completo de respiracao

GLOW_RGB = (70, 190, 255)     # halo azul
CORE_RGB = (248, 253, 255)    # o desenho em si, quase branco


def render_svg(px):
    import cairosvg
    tmp = "/tmp/_logo_%d.png" % px
    cairosvg.svg2png(url=SVG, write_to=tmp, output_width=px, output_height=px)
    im = Image.open(tmp).convert("RGBA")
    bb = im.getchannel("A").getbbox()
    return im.crop(bb) if bb else im


def build(frame):
    """frame: 0..N_FRAMES-1, um ciclo completo de respiracao."""
    ph = frame / float(N_FRAMES)
    # cosseno: a respiracao desacelera nos extremos, como um folego real.
    # Com rampa linear o movimento pareceria mecanico.
    c01 = 0.5 - 0.5 * math.cos(2 * math.pi * ph)

    size = int(round(BASE * (SCALE_MIN + (SCALE_MAX - SCALE_MIN) * c01)))
    art = render_svg(size)

    canvas = Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0))
    canvas.alpha_composite(art, ((CANVAS - art.width) // 2,
                                 (CANVAS - art.height) // 2))

    a = np.asarray(canvas.getchannel("A"), np.float32) / 255.0

    src = Image.fromarray((a * 255).astype(np.uint8), "L")
    h1 = np.asarray(src.filter(ImageFilter.GaussianBlur(CANVAS * 0.030)),
                    np.float32) / 255.0
    h2 = np.asarray(src.filter(ImageFilter.GaussianBlur(CANVAS * 0.105)),
                    np.float32) / 255.0
    halo = np.clip(h1 * (0.55 + 0.35 * c01) + h2 * (0.62 + 0.48 * c01), 0, 1)

    # UMA rampa 1D: misturar duas cores livremente gerava 400+ combinacoes e
    # estourava a paleta de 256 do L8.
    t = np.clip(halo * 0.55 + a * 1.30, 0, 1)
    LEV = 46
    t = np.clip(np.round(t * (LEV - 1)) / (LEV - 1), 0, 1)

    lo = np.array((10, 40, 96), np.float32)
    mid = np.array(GLOW_RGB, np.float32)
    hi = np.array(CORE_RGB, np.float32)
    p = np.clip(t / 0.52, 0, 1)[..., None]
    q2 = np.clip((t - 0.52) / 0.48, 0, 1)[..., None]
    rgb = lo * (1 - p) + mid * p
    rgb = rgb * (1 - q2) + hi * q2
    rgb = rgb * (0.86 + 0.14 * c01)

    alpha = np.clip(t * 1.22, 0, 1)
    return Image.fromarray(
        np.dstack([rgb, alpha * 255]).astype(np.uint8), "RGBA")


def build_arc(frame):
    ph = frame / float(N_FRAMES)
    c01 = 0.5 - 0.5 * math.cos(2 * math.pi * ph)

    src = Image.open(ARC).convert("RGBA")
    w = int(round(ARC_W * (SCALE_MIN + (SCALE_MAX - SCALE_MIN) * c01)))
    h = max(1, int(round(src.height * w / src.width)))
    art = src.resize((w, h), Image.LANCZOS)

    canvas = Image.new("RGBA", (ARC_W + 10, ARC_CANVAS_H), (0, 0, 0, 0))
    canvas.alpha_composite(art, ((canvas.width - w) // 2,
                                 (canvas.height - h) // 2))

    a = np.asarray(canvas.getchannel("A"), np.float32) / 255.0
    g = Image.fromarray((a * 255).astype(np.uint8), "L")
    h1 = np.asarray(g.filter(ImageFilter.GaussianBlur(0.8)), np.float32) / 255.0
    h2 = np.asarray(g.filter(ImageFilter.GaussianBlur(2.4)), np.float32) / 255.0

    t = np.clip(a * 1.15 + h1 * (0.40 + 0.25 * c01) + h2 * (0.45 + 0.30 * c01), 0, 1)
    LEV = 40
    t = np.clip(np.round(t * (LEV - 1)) / (LEV - 1), 0, 1)

    lo = np.array((10, 40, 96), np.float32)
    mid = np.array(GLOW_RGB, np.float32)
    hi = np.array(CORE_RGB, np.float32)
    p = np.clip(t / 0.52, 0, 1)[..., None]
    q2 = np.clip((t - 0.52) / 0.48, 0, 1)[..., None]
    rgb = lo * (1 - p) + mid * p
    rgb = rgb * (1 - q2) + hi * q2
    rgb = rgb * (0.86 + 0.14 * c01)

    return Image.fromarray(
        np.dstack([rgb, np.clip(t * 1.20, 0, 1) * 255]).astype(np.uint8), "RGBA")


def main():
    os.makedirs(OUT, exist_ok=True)
    os.makedirs(_os.path.join(_HERE, "preview"), exist_ok=True)

    total, worst = 0, 0
    for i in range(N_FRAMES):
        im = build(i)
        im.save(f"{OUT}/logo_{i:02d}.png")
        total += CANVAS * CANVAS + 1024
        worst = max(worst, len(np.unique(np.asarray(im).reshape(-1, 4), axis=0)))

    for i in range(N_FRAMES):
        im = build_arc(i)
        im.save(f"{OUT}/arc_{i:02d}.png")
        total += im.width * im.height + 1024
        worst = max(worst, len(np.unique(np.asarray(im).reshape(-1, 4), axis=0)))

    a0 = Image.open(f"{OUT}/arc_00.png")
    print(f"  {N_FRAMES} frames de {CANVAS}x{CANVAS}")
    print(f"  arco: {N_FRAMES} frames de {a0.width}x{a0.height}")
    print(f"  pior paleta: {worst} RGBA  {'L8 ok' if worst <= 256 else 'ESTOUROU'}")
    print(f"  flash: {total/1024:.0f} KB em L8_ARGB8888")
    print("\n  --- para o .cpp ---")
    print(f"    #define LOGO_FRAMES {N_FRAMES}")
    print(f"    static const int16_t LOGO_PX = {CANVAS};")
    print(f"    static const int16_t ARC_W = {a0.width}, ARC_H = {a0.height};")

    SHOW = 10
    sh = Image.new("RGBA", (SHOW * 84, 84), (3, 10, 26, 255))
    for k in range(SHOW):
        fi = int(k * N_FRAMES / SHOW)
        lg = Image.open(f"{OUT}/logo_{fi:02d}.png")
        ar = Image.open(f"{OUT}/arc_{fi:02d}.png")
        sh.alpha_composite(lg, (k * 84 + (84 - lg.width) // 2, 4))
        sh.alpha_composite(ar, (k * 84 + (84 - ar.width) // 2, 4 + CANVAS - 4))
    sh.convert("RGB").resize((SHOW * 168, 168), Image.LANCZOS).save(
        _os.path.join(_HERE, "preview") + "/logo.png")


if __name__ == "__main__":
    main()
