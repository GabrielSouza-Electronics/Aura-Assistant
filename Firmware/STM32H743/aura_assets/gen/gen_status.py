#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Barra de status - Aura Assistant.

Gera:
    status/aura_mark.png        marca "AURA" com arco e estrela
    status/wifi_0..3.png        4 niveis de sinal (0 = sem conexao)
    status/batt_frame.png       contorno da bateria, vazio
    status/batt_bolt.png        raio sobreposto quando carregando

O PREENCHIMENTO da bateria nao e asset: e um Box desenhado por codigo, que
cresce com o nivel e muda de cor conforme o carregador. Gerar 100 estagios
seria desperdicio, e com Box a cor fica livre - o DMA2D preenche retangulo
solido por registrador, sem tocar na flash.

O "A" e um chevron sem barra horizontal, como na referencia: mais tecnico e
combina com a linguagem chanfrada dos icones.
"""

import os, math
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

import os as _os
_HERE = _os.path.dirname(_os.path.abspath(__file__))
OUT = _os.path.join(_HERE, "status")

SS = 6
C_LOW, C_MID, C_HIGH = (16, 76, 165), (86, 200, 252), (243, 253, 255)


def colorize(t, levels=190, lo=None, mid=None, hi=None):
    t = np.clip(t, 0, 1).astype(np.float32)
    t = np.clip(np.round(t * (levels - 1)) / (levels - 1), 0, 1)
    lo = np.array(lo or C_LOW); mid = np.array(mid or C_MID); hi = np.array(hi or C_HIGH)
    a = np.clip(t / 0.48, 0, 1)[..., None]
    b = np.clip((t - 0.48) / 0.52, 0, 1)[..., None]
    rgb = lo * (1 - a) + mid * a
    rgb = rgb * (1 - b) + hi * b
    return Image.fromarray(
        np.dstack([rgb, np.clip(t * 1.28, 0, 1) * 255]).astype(np.uint8), "RGBA")


def finish(img, w, h, b1=1.1, k1=0.50, b2=4.0, k2=0.62, gain=1.06, **kw):
    small = img.resize((w, h), Image.LANCZOS)
    base = np.asarray(small, np.float32) / 255.0
    g1 = np.asarray(small.filter(ImageFilter.GaussianBlur(b1)), np.float32) / 255.0
    g2 = np.asarray(small.filter(ImageFilter.GaussianBlur(b2)), np.float32) / 255.0
    return colorize(np.clip(base * gain + g1 * k1 + g2 * k2, 0, 1), **kw)


# ---------------------------------------------------------------- AURA
MARK_W, MARK_H = 148, 46


def build_mark():
    w, h = MARK_W * SS, MARK_H * SS
    img = Image.new("L", (w, h), 0)
    dr = ImageDraw.Draw(img)
    st = max(1, int(1.7 * SS))

    # letras desenhadas a mao. O "A" e um chevron sem barra, e o "R" tem
    # perna reta - as duas coisas que separam esta marca de uma fonte pronta.
    top, bot = 0.30 * h, 0.64 * h
    H = bot - top

    def A(x, wd):
        dr.line([(x, bot), (x + wd / 2, top), (x + wd, bot)],
                fill=255, width=st, joint="curve")

    def U(x, wd):
        r = wd / 2.0
        dr.line([(x, top), (x, bot - r)], fill=255, width=st)
        dr.line([(x + wd, top), (x + wd, bot - r)], fill=255, width=st)
        dr.arc([x, bot - 2 * r, x + wd, bot], 0, 180, fill=255, width=st)

    def R(x, wd):
        r = H * 0.27                      # raio do bojo, menor que meia altura
        dr.line([(x, top), (x, bot)], fill=255, width=st)          # haste
        dr.line([(x, top), (x + wd - r, top)], fill=255, width=st)  # topo
        dr.arc([x + wd - 2 * r, top, x + wd, top + 2 * r],
               -90, 90, fill=255, width=st)                         # bojo
        dr.line([(x, top + 2 * r), (x + wd - r, top + 2 * r)],
                fill=255, width=st)                                 # base do bojo
        dr.line([(x + wd * 0.46, top + 2 * r), (x + wd, bot)],
                fill=255, width=st)                                 # perna

    gap = 0.058 * w
    wd = 0.122 * w
    x = (w - (4 * wd + 3 * gap)) / 2
    for fn in (A, U, R, A):
        fn(x, wd)
        x += wd + gap

    # arco sob as letras
    dr.arc([w * 0.26, h * 0.62, w * 0.74, h * 0.99], 0, 180,
           fill=180, width=max(1, int(1.3 * SS)))

    # estrela de quatro pontas acima
    cx, cy = w / 2, h * 0.14
    for (lx, ly) in ((w * 0.075, h * 0.012), (w * 0.014, h * 0.075)):
        dr.polygon([(cx - lx, cy), (cx, cy - ly), (cx + lx, cy), (cx, cy + ly)],
                   fill=255)

    return finish(img, MARK_W, MARK_H, b1=1.3, k1=0.55, b2=6.0, k2=0.80)


# ---------------------------------------------------------------- WiFi
WIFI = 22


def build_wifi(level):
    """
    BARRAS VERTICAIS em vez de arcos concentricos.

    Os arcos tinham dois problemas: sao curvos, entao nao alinham com nada
    ao redor, e num tamanho pequeno as tres curvas viram um borrao. Barras
    retas sao simetricas, compartilham a mesma linha de base e a contagem e
    lida de relance - que e tudo o que um indicador de sinal precisa.
    """
    n = WIFI * SS
    img = Image.new("L", (n, n), 0)
    dr = ImageDraw.Draw(img)

    bars = 3
    bw = n * 0.20                  # largura da barra
    gap = n * 0.10
    total = bars * bw + (bars - 1) * gap
    x0 = (n - total) / 2
    base = n * 0.80
    for i in range(bars):
        h = n * (0.26 + 0.22 * i)  # altura crescente
        x = x0 + i * (bw + gap)
        v = 255 if level >= i + 1 else 38
        dr.rectangle([x, base - h, x + bw, base], fill=v)

    if level == 0:
        st = max(1, int(1.7 * SS))
        d = n * 0.26
        dr.line([n / 2 - d, n / 2 - d, n / 2 + d, n / 2 + d], fill=255, width=st)

    return finish(img, WIFI, WIFI)


# ---------------------------------------------------------------- bateria
BATT_W, BATT_H = 30, 15


def build_batt_frame():
    w, h = BATT_W * SS, BATT_H * SS
    img = Image.new("L", (w, h), 0)
    dr = ImageDraw.Draw(img)
    st = max(1, int(1.5 * SS))
    b = 1.5 * SS
    x1 = w - 5 * SS
    c = 1.4 * SS                    # chanfro discreto, so para nao ficar duro
    pts = [(b + c, b), (x1 - c, b), (x1, b + c), (x1, h - b - c),
           (x1 - c, h - b), (b + c, h - b), (b, h - b - c), (b, b + c)]
    dr.line([p for p in pts] + [pts[0]], fill=255, width=st, joint="curve")
    # terminal: centrado verticalmente e simetrico
    dr.rectangle([x1 + 1.2 * SS, h * 0.34, w - 1.2 * SS, h * 0.66], fill=210)
    return finish(img, BATT_W, BATT_H)


def build_bolt():
    w, h = BATT_W * SS, BATT_H * SS
    img = Image.new("L", (w, h), 0)
    dr = ImageDraw.Draw(img)
    cx, cy = (w - 5 * SS) / 2, h / 2
    s = h * 0.30
    dr.polygon([(cx + s * 0.55, cy - s), (cx - s * 0.30, cy + s * 0.12),
                (cx + s * 0.10, cy + s * 0.12), (cx - s * 0.55, cy + s),
                (cx + s * 0.35, cy - s * 0.10), (cx - s * 0.05, cy - s * 0.10)],
               fill=255)
    return finish(img, BATT_W, BATT_H)


def main():
    os.makedirs(OUT, exist_ok=True)
    os.makedirs(_os.path.join(_HERE, "preview"), exist_ok=True)

    total, worst = 0, 0

    def save(im, name, px):
        nonlocal total, worst
        im.save(f"{OUT}/{name}.png")
        total += px + 1024
        worst = max(worst, len(np.unique(np.asarray(im).reshape(-1, 4), axis=0)))

    save(build_mark(), "aura_mark", MARK_W * MARK_H)
    for lv in range(4):
        save(build_wifi(lv), f"wifi_{lv}", WIFI * WIFI)
    save(build_batt_frame(), "batt_frame", BATT_W * BATT_H)
    save(build_bolt(), "batt_bolt", BATT_W * BATT_H)

    print(f"  aura_mark {MARK_W}x{MARK_H}, wifi 0..3 {WIFI}x{WIFI}, "
          f"bateria {BATT_W}x{BATT_H}")
    print(f"  pior paleta: {worst} RGBA  {'L8 ok' if worst <= 256 else 'ESTOUROU'}")
    print(f"  flash: {total/1024:.0f} KB em L8_ARGB8888")

    sh = Image.new("RGBA", (480, 130), (3, 10, 26, 255))
    sh.alpha_composite(Image.open(f"{OUT}/aura_mark.png"), (166, 4))
    for lv in range(4):
        sh.alpha_composite(Image.open(f"{OUT}/wifi_{lv}.png"), (28 + lv * 30, 66))
    sh.alpha_composite(Image.open(f"{OUT}/batt_frame.png"), (280, 70))
    sh.alpha_composite(Image.open(f"{OUT}/batt_frame.png"), (340, 70))
    sh.alpha_composite(Image.open(f"{OUT}/batt_bolt.png"), (340, 70))
    sh.convert("RGB").resize((960, 260), Image.LANCZOS).save(
        _os.path.join(_HERE, "preview") + "/status.png")


if __name__ == "__main__":
    main()
