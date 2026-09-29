#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Logo principal com aura - Aura Assistant.

Este e o logo GRANDE da tela de espera, diferente do pequeno da barra de
status. Ele tem quatro camadas, e a ordem importa:

  1. aura externa   gaussiana muito larga e fraca, cria o campo de luz
  2. disco interno  a "esfera" onde o simbolo vive, com gradiente proprio
  3. aneis          duas circunferencias finas concentricas, marcando o
                    limite do disco - sao o que da a leitura de objeto
                    construido em vez de mancha luminosa
  4. simbolo        o desenho em branco, com halo curto

Sem a camada 3 o conjunto vira um borrao brilhante. E ela que faz o olho
entender que ha uma superficie ali.

Saida: hero/hero_<frame>.png
"""

import os, math
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

import os as _os
_HERE = _os.path.dirname(_os.path.abspath(__file__))
OUT = _os.path.join(_HERE, "hero")

SVG = "/mnt/user-data/uploads/gabriel_alves_icon_blender_ready.svg"

CANVAS = 200           # o sprite inteiro, com a aura
DISC = 92              # diametro do disco interno
SYM = 54               # o simbolo dentro do disco
FRAMES = 24            # respiracao
SCALE_MIN, SCALE_MAX = 1.00, 1.06

SS = 4


def render_svg(px):
    import cairosvg
    tmp = "/tmp/_hero_%d.png" % px
    cairosvg.svg2png(url=SVG, write_to=tmp, output_width=px, output_height=px)
    im = Image.open(tmp).convert("RGBA")
    bb = im.getchannel("A").getbbox()
    return im.crop(bb) if bb else im


def build(frame):
    ph = frame / float(FRAMES)
    b = 0.5 - 0.5 * math.cos(2 * math.pi * ph)     # respiracao

    n = CANVAS * SS
    c = n / 2.0
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float32)
    d = np.sqrt((xx - c) ** 2 + (yy - c) ** 2) / SS   # px finais

    # --- 1. aura externa ---
    # A aura e uma CASCA, nao uma gaussiana cheia: ela some no miolo, onde
    # fica o simbolo. Antes ela somava por cima do desenho e saturava a
    # contra-forma do "A" no pico da respiracao.
    shell = np.exp(-((d - DISC * 0.86) / (CANVAS * 0.085)) ** 2)
    aura = shell * (0.30 + 0.18 * b)
    aura += np.exp(-((d - DISC * 0.60) / (CANVAS * 0.030)) ** 2) * (0.26 + 0.14 * b)

    # --- 2. disco interno: mais claro no topo, como uma esfera iluminada ---
    R = DISC * 0.5
    inside = np.clip((R - d) / 3.0, 0, 1)
    ny = (yy / SS - CANVAS / 2) / R
    disc = inside * (0.06 + 0.16 * np.clip(0.55 - ny * 0.75, 0, 1))

    # borda do disco acende: e o terminador da esfera
    rim = np.exp(-((d - R) / 1.7) ** 2) * (0.92 + 0.12 * b)
    rim += np.exp(-((d - R) / 5.5) ** 2) * (0.22 + 0.10 * b)

    # --- 3. aneis concentricos, finos ---
    rings = np.zeros_like(d)
    for rr, k in ((1.20, 0.30), (1.42, 0.18)):
        # os aneis EXPANDEM levemente: a respiracao vive aqui, longe do
        # simbolo, onde o movimento nao compromete nitidez
        rr2 = rr * (1.0 + 0.035 * b)
        rings += np.exp(-((d - R * rr2) / 1.1) ** 2) * k * (0.80 + 0.35 * b)

    img = np.clip(aura + disc + rim + rings, 0, 1.5)

    # --- 4. simbolo ---
    # O simbolo tem tamanho FIXO e nitidez constante. Antes ele escalava e o
    # halo pulsava sobre ele, o que borrava as hastes a cada respiracao - de
    # perto lia como imagem mal renderizada. Agora quem respira e so a aura
    # atras; o desenho fica cravado.
    sym_px = SYM
    art = render_svg(sym_px * SS)
    layer = Image.new("L", (n, n), 0)
    layer.paste(art.getchannel("A"), ((n - art.width) // 2, (n - art.height) // 2))
    sym = np.asarray(layer, np.float32) / 255.0
    # halo do simbolo: FIXO, so para descolar do disco. O que varia com a
    # respiracao e a aura externa, calculada antes.
    sg = np.asarray(layer.filter(ImageFilter.GaussianBlur(1.6 * SS)),
                    np.float32) / 255.0
    img = img + sg * 0.46

    small = Image.fromarray((np.clip(img, 0, 1) * 255).astype(np.uint8), "L")
    small = small.resize((CANVAS, CANVAS), Image.LANCZOS)
    a = np.asarray(small, np.float32) / 255.0

    sl = Image.fromarray((np.clip(sym, 0, 1) * 255).astype(np.uint8), "L")
    sl = sl.resize((CANVAS, CANVAS), Image.LANCZOS)
    smask = np.asarray(sl, np.float32) / 255.0

    # queda a zero na borda do canvas
    yy2, xx2 = np.mgrid[0:CANVAS, 0:CANVAS].astype(np.float32)
    d2 = np.sqrt((xx2 - CANVAS / 2) ** 2 + (yy2 - CANVAS / 2) ** 2)
    a = a * np.clip((CANVAS * 0.49 - d2) / (CANVAS * 0.09), 0, 1)

    LEV = 190
    a = np.clip(np.round(np.clip(a, 0, 1) * (LEV - 1)) / (LEV - 1), 0, 1)
    smask = np.clip(np.round(smask * 12) / 12, 0, 1)

    # rampa: azul profundo -> ciano -> branco
    lo = np.array((6, 30, 92), np.float32)
    mid = np.array((60, 168, 245), np.float32)
    hi = np.array((228, 248, 255), np.float32)
    p = np.clip(a / 0.48, 0, 1)[..., None]
    q = np.clip((a - 0.48) / 0.52, 0, 1)[..., None]
    rgb = lo * (1 - p) + mid * p
    rgb = rgb * (1 - q) + hi * q

    # O simbolo entra como PICO da propria rampa, nao como cor separada.
    # Misturar branco puro por cima do gradiente gerava 450+ combinacoes e
    # estourava a paleta de 256 do L8.
    a = np.clip(np.maximum(a, smask * 0.97), 0, 1)
    a = np.clip(np.round(a * (LEV - 1)) / (LEV - 1), 0, 1)
    p = np.clip(a / 0.48, 0, 1)[..., None]
    q = np.clip((a - 0.48) / 0.52, 0, 1)[..., None]
    rgb = lo * (1 - p) + mid * p
    rgb = rgb * (1 - q) + hi * q
    alpha = np.clip(a * 1.20, 0, 1)

    return Image.fromarray(
        np.dstack([rgb, alpha * 255]).astype(np.uint8), "RGBA")


def main():
    os.makedirs(OUT, exist_ok=True)
    os.makedirs(_os.path.join(_HERE, "preview"), exist_ok=True)

    total, worst = 0, 0
    for i in range(FRAMES):
        im = build(i)
        im.save(f"{OUT}/hero_{i:02d}.png")
        total += CANVAS * CANVAS + 1024
        worst = max(worst, len(np.unique(np.asarray(im).reshape(-1, 4), axis=0)))

    print(f"  {FRAMES} frames de {CANVAS}x{CANVAS}")
    print(f"  pior paleta: {worst} RGBA  {'L8 ok' if worst <= 256 else 'ESTOUROU'}")
    print(f"  flash: {total/1024:.0f} KB em L8_ARGB8888")

    sh = Image.new("RGBA", (CANVAS * 4, CANVAS), (2, 6, 18, 255))
    for k, i in enumerate((0, 6, 12, 18)):
        sh.alpha_composite(Image.open(f"{OUT}/hero_{i:02d}.png"), (k * CANVAS, 0))
    sh.convert("RGB").save(_os.path.join(_HERE, "preview") + "/hero.png")


if __name__ == "__main__":
    main()
