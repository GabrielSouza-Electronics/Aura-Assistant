#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Sistema de icones e anel de progresso - Aura Assistant.

LINGUAGEM DE DESIGN (as regras valem para todos os icones; e a consistencia
delas que faz um conjunto parecer projetado em vez de coletado):

  1. GRID 24. Tudo e desenhado num quadrado normalizado pensado como 24
     unidades. As coordenadas caem em multiplos de 1/24, entao os tracos se
     alinham entre icones diferentes.

  2. CANTOS CHANFRADOS, nao arredondados. Um corte em 45 graus le como
     tecnico; um raio de canto le como app de celular. E a diferenca mais
     barata entre "generico" e "HMI".

  3. HIERARQUIA INTERNA. Cada icone tem traco primario (100% de brilho) e
     detalhe secundario (55%). Sem isso o icone vira silhueta chapada e tudo
     dentro dele parece igualmente importante.

  4. GAPS no traco. Interromper a linha em um ou dois pontos e vocabulario
     de HUD: sugere desenho tecnico, nao pictograma.

  5. TRACO PROPORCIONAL. A espessura acompanha o tamanho do icone.

Saida:
    icons/<nome>_<tamanho>.png     5 icones x 3 tamanhos
    ring/ring_<nn>.png             anel de progresso segmentado, 16 estagios
    ring/ring_idle.png             anel de selecao, sem progresso
"""

import os, math
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

import os as _os
_HERE = _os.path.dirname(_os.path.abspath(__file__))
OUT = _os.path.join(_HERE, "icons")
OUT_RING = _os.path.join(_HERE, "ring")

SIZES = [36, 48, 64]
SS = 6
NAMES = ["settings", "tasks", "reminders", "calendar", "chat"]

C_LOW, C_MID, C_HIGH = (16, 72, 158), (78, 196, 252), (240, 253, 255)

DIM = 0.55          # brilho do detalhe secundario
G = 1.0 / 24.0      # uma unidade do grid


def chamfer_rect(dr, u, x0, y0, x1, y1, c, v, w):
    """Retangulo com cantos cortados em 45 graus."""
    pts = [(x0 + c, y0), (x1 - c, y0), (x1, y0 + c), (x1, y1 - c),
           (x1 - c, y1), (x0 + c, y1), (x0, y1 - c), (x0, y0 + c)]
    dr.line([(u(a), u(b)) for a, b in pts] + [(u(pts[0][0]), u(pts[0][1]))],
            fill=v, width=w, joint="curve")


def draw_settings(dr, u, w):
    """
    Engrenagem de verdade: 8 dentes trapezoidais em torno de um anel.
    A versao anterior era um hexagono com hastes soltas e nao lia como
    engrenagem - parecia um simbolo abstrato.
    """
    cx, cy = 0.5, 0.5
    R_IN = 6.4 * G          # raio da base dos dentes
    R_OUT = 9.6 * G         # ponta do dente
    TEETH = 8
    half = math.pi / TEETH * 0.30   # meia-largura angular da ponta
    base = math.pi / TEETH * 0.62   # meia-largura na base

    pts = []
    for i in range(TEETH):
        a = 2 * math.pi * i / TEETH
        pts += [(cx + R_IN * math.cos(a - base), cy + R_IN * math.sin(a - base)),
                (cx + R_OUT * math.cos(a - half), cy + R_OUT * math.sin(a - half)),
                (cx + R_OUT * math.cos(a + half), cy + R_OUT * math.sin(a + half)),
                (cx + R_IN * math.cos(a + base), cy + R_IN * math.sin(a + base))]
        # arco ate o proximo dente
        nxt = 2 * math.pi * (i + 1) / TEETH
        for k in range(1, 4):
            t = a + base + (nxt - base - (a + base)) * k / 4.0
            pts.append((cx + R_IN * math.cos(t), cy + R_IN * math.sin(t)))

    dr.line([(u(x), u(y)) for x, y in pts] + [(u(pts[0][0]), u(pts[0][1]))],
            fill=255, width=w, joint="curve")

    # furo central e anel interno secundario
    r = 3.0 * G
    dr.ellipse([u(cx - r), u(cy - r), u(cx + r), u(cy + r)], outline=255, width=w)
    r2 = 4.6 * G
    dr.ellipse([u(cx - r2), u(cy - r2), u(cx + r2), u(cy + r2)],
               outline=int(255 * DIM), width=w)


def draw_tasks(dr, u, w):
    v = int(255 * DIM)
    for k, y in enumerate((0.26, 0.50, 0.74)):
        b = 3.5 * G
        s = 2.1 * G          # meia-altura: 2*s = 4.2G < 5.8G de espacamento
        if k < 2:
            chamfer_rect(dr, u, b, y - s, b + 2 * s, y + s, 0.8 * G, 255, w)
            dr.line([u(b + 0.9 * G), u(y + 0.1 * G), u(b + 1.8 * G), u(y + 1.2 * G),
                     u(b + 3.4 * G), u(y - 1.4 * G)], fill=255, width=w, joint="curve")
        else:
            chamfer_rect(dr, u, b, y - s, b + 2 * s, y + s, 0.8 * G, v, w)
        dr.line([u(10 * G), u(y), u(15.5 * G), u(y)], fill=255 if k < 2 else v, width=w)
        dr.line([u(17 * G), u(y), u(20.5 * G), u(y)], fill=v, width=w)


def draw_reminders(dr, u, w):
    """Sino com ondas de alerta nas laterais."""
    v = int(255 * DIM)
    dr.line([u(7 * G), u(15 * G), u(7 * G), u(11 * G)], fill=255, width=w)
    dr.line([u(17 * G), u(15 * G), u(17 * G), u(11 * G)], fill=255, width=w)
    dr.arc([u(7 * G), u(5 * G), u(17 * G), u(17 * G)], 180, 360, fill=255, width=w)
    dr.line([u(5 * G), u(15 * G), u(19 * G), u(15 * G)], fill=255, width=w)
    dr.arc([u(10 * G), u(15 * G), u(14 * G), u(19.5 * G)], 0, 180, fill=255, width=w)
    dr.line([u(12 * G), u(3.5 * G), u(12 * G), u(5.5 * G)], fill=255, width=w)
    for sg in (-1, 1):
        for r in (3.0, 5.2):
            cxw = 12 * G + sg * 5.0 * G
            box = [u(cxw - r * G), u(10 * G - r * G), u(cxw + r * G), u(10 * G + r * G)]
            dr.arc(box, -50 if sg > 0 else 130, 50 if sg > 0 else 230, fill=v, width=w)


def draw_calendar(dr, u, w):
    v = int(255 * DIM)
    chamfer_rect(dr, u, 3 * G, 6 * G, 21 * G, 20 * G, 2 * G, 255, w)
    dr.line([u(3 * G), u(10.5 * G), u(21 * G), u(10.5 * G)], fill=255, width=w)
    dr.line([u(8 * G), u(3.5 * G), u(8 * G), u(8 * G)], fill=255, width=w)
    dr.line([u(16 * G), u(3.5 * G), u(16 * G), u(8 * G)], fill=255, width=w)
    for r, gy in enumerate((14.0, 17.4)):
        for c, gx in enumerate((7.0, 12.0, 17.0)):
            s = 1.5 * G
            if r == 0 and c == 1:
                chamfer_rect(dr, u, gx * G - s, gy * G - s, gx * G + s, gy * G + s,
                             0.6 * G, 255, w)
            else:
                dr.line([u(gx * G - s * 0.7), u(gy * G),
                         u(gx * G + s * 0.7), u(gy * G)], fill=v, width=w)


def draw_chat(dr, u, w):
    v = int(255 * DIM)
    chamfer_rect(dr, u, 3 * G, 5 * G, 21 * G, 16 * G, 2.4 * G, 255, w)
    dr.line([u(8 * G), u(16 * G), u(8 * G), u(20.5 * G), u(13 * G), u(16 * G)],
            fill=255, width=w, joint="curve")
    for k, (y, x1) in enumerate(((9.0, 17.0), (12.0, 14.0))):
        dr.line([u(7 * G), u(y * G), u(x1 * G), u(y * G)],
                fill=255 if k == 0 else v, width=w)


DRAW = {"settings": draw_settings, "tasks": draw_tasks,
        "reminders": draw_reminders, "calendar": draw_calendar, "chat": draw_chat}


def colorize(t, levels=190):
    t = np.clip(t, 0, 1).astype(np.float32)
    t = np.clip(np.round(t * (levels - 1)) / (levels - 1), 0, 1)
    lo, mid, hi = map(np.array, (C_LOW, C_MID, C_HIGH))
    a = np.clip(t / 0.48, 0, 1)[..., None]
    b = np.clip((t - 0.48) / 0.52, 0, 1)[..., None]
    rgb = lo * (1 - a) + mid * a
    rgb = rgb * (1 - b) + hi * b
    return Image.fromarray(
        np.dstack([rgb, np.clip(t * 1.28, 0, 1) * 255]).astype(np.uint8), "RGBA")


def build_icon(name, n):
    img = Image.new("L", (n * SS, n * SS), 0)
    dr = ImageDraw.Draw(img)
    u = lambda v: v * n * SS
    w = max(1, int(round(0.046 * n * SS)))
    DRAW[name](dr, u, w)

    small = img.resize((n, n), Image.LANCZOS)
    base = np.asarray(small, np.float32) / 255.0
    g1 = np.asarray(small.filter(ImageFilter.GaussianBlur(1.2)), np.float32) / 255.0
    g2 = np.asarray(small.filter(ImageFilter.GaussianBlur(4.2)), np.float32) / 255.0
    return colorize(np.clip(base * 1.08 + g1 * 0.50 + g2 * 0.70, 0, 1))


# ---------------------------------------------------------------- anel
RING_N = 16
RING_PX = 84
RING_SEG = 32


def build_ring(step, idle=False):
    """
    Anel SEGMENTADO em vez de arco continuo: um arco liso le como barra de
    progresso de app; segmentos discretos leem como instrumento. Tem tambem
    um anel externo fino e quatro marcas cardeais, que dao ar de escala
    calibrada.
    """
    n = RING_PX * SS
    img = Image.new("L", (n, n), 0)
    dr = ImageDraw.Draw(img)
    c = n / 2
    R = n * 0.40
    w = max(1, int(RING_PX * SS * 0.028))

    dr.ellipse([c - R * 1.22, c - R * 1.22, c + R * 1.22, c + R * 1.22],
               outline=70, width=max(1, w // 2))
    for i in range(4):
        a = math.pi / 2 * i - math.pi / 2
        dr.line([c + R * 1.12 * math.cos(a), c + R * 1.12 * math.sin(a),
                 c + R * 1.34 * math.cos(a), c + R * 1.34 * math.sin(a)],
                fill=150, width=w)

    filled = 0 if idle else int(round(RING_SEG * step / float(RING_N)))
    for i in range(RING_SEG):
        a0 = -math.pi / 2 + 2 * math.pi * i / RING_SEG
        a1 = a0 + 2 * math.pi / RING_SEG * 0.62
        v = 255 if i < filled else (60 if idle else 26)
        dr.arc([c - R, c - R, c + R, c + R],
               math.degrees(a0), math.degrees(a1), fill=v, width=w * 2)

    if 0 < filled < RING_SEG:
        a = -math.pi / 2 + 2 * math.pi * filled / RING_SEG
        r2 = w * 1.7
        dr.ellipse([c + R * math.cos(a) - r2, c + R * math.sin(a) - r2,
                    c + R * math.cos(a) + r2, c + R * math.sin(a) + r2], fill=255)

    small = img.resize((RING_PX, RING_PX), Image.LANCZOS)
    base = np.asarray(small, np.float32) / 255.0
    g1 = np.asarray(small.filter(ImageFilter.GaussianBlur(1.4)), np.float32) / 255.0
    g2 = np.asarray(small.filter(ImageFilter.GaussianBlur(5.0)), np.float32) / 255.0
    return colorize(np.clip(base * 1.05 + g1 * 0.55 + g2 * 0.65, 0, 1))


# ---------------------------------------------------------------- setas
# Indicam o gesto vertical em curso. Ficam visiveis so enquanto a mao esta
# acima do limiar, entao funcionam como confirmacao de que o sistema
# registrou o movimento antes de a acao disparar.
ARROW_W, ARROW_H = 40, 22


def build_arrow(up):
    w, h = ARROW_W * SS, ARROW_H * SS
    img = Image.new("L", (w, h), 0)
    dr = ImageDraw.Draw(img)
    st = max(1, int(ARROW_H * SS * 0.11))
    pad = h * 0.22
    if up:
        pts = [(w * 0.16, h - pad), (w * 0.5, pad), (w * 0.84, h - pad)]
    else:
        pts = [(w * 0.16, pad), (w * 0.5, h - pad), (w * 0.84, pad)]
    dr.line(pts, fill=255, width=st, joint="curve")
    # segunda seta menor atras, dando direcao
    sc, oy = 0.55, (h * 0.30 if up else -h * 0.30)
    pts2 = [(w * 0.5 + (x - w * 0.5) * sc, h * 0.5 + (y - h * 0.5) * sc + oy)
            for x, y in pts]
    dr.line(pts2, fill=110, width=max(1, int(st * 0.8)), joint="curve")

    small = img.resize((ARROW_W, ARROW_H), Image.LANCZOS)
    base = np.asarray(small, np.float32) / 255.0
    g1 = np.asarray(small.filter(ImageFilter.GaussianBlur(1.2)), np.float32) / 255.0
    g2 = np.asarray(small.filter(ImageFilter.GaussianBlur(4.0)), np.float32) / 255.0
    return colorize(np.clip(base * 1.08 + g1 * 0.50 + g2 * 0.62, 0, 1))


def main():
    os.makedirs(OUT, exist_ok=True)
    os.makedirs(OUT_RING, exist_ok=True)
    os.makedirs(_os.path.join(_HERE, "preview"), exist_ok=True)

    total, worst = 0, 0
    for name in NAMES:
        for n in SIZES:
            im = build_icon(name, n)
            im.save(f"{OUT}/{name}_{n}.png")
            total += n * n + 1024
            worst = max(worst, len(np.unique(np.asarray(im).reshape(-1, 4), axis=0)))
    print(f"  {len(NAMES)} icones x {len(SIZES)} tamanhos = {len(NAMES)*len(SIZES)} imagens")

    for up in (True, False):
        im = build_arrow(up)
        im.save(f"{OUT_RING}/arrow_{'up' if up else 'down'}.png")
        total += ARROW_W * ARROW_H + 1024
        worst = max(worst, len(np.unique(np.asarray(im).reshape(-1, 4), axis=0)))
    print(f"  setas: 2 x {ARROW_W}x{ARROW_H}")

    build_ring(0, idle=True).save(f"{OUT_RING}/ring_idle.png")
    total += RING_PX * RING_PX + 1024
    for k in range(RING_N):
        im = build_ring(k + 1)
        im.save(f"{OUT_RING}/ring_{k:02d}.png")
        total += RING_PX * RING_PX + 1024
        worst = max(worst, len(np.unique(np.asarray(im).reshape(-1, 4), axis=0)))
    print(f"  anel: {RING_N} estagios + idle, {RING_PX}x{RING_PX}")
    print(f"  pior paleta: {worst} RGBA  {'L8 ok' if worst <= 256 else 'ESTOUROU'}")
    print(f"  flash: {total/1024:.0f} KB em L8_ARGB8888")

    sh = Image.new("RGBA", (64 * 5, 64 * 3 + 100), (4, 12, 30, 255))
    for c, name in enumerate(NAMES):
        for r, n in enumerate(SIZES):
            im = Image.open(f"{OUT}/{name}_{n}.png")
            sh.alpha_composite(im, (c * 64 + (64 - n) // 2, r * 64 + (64 - n) // 2))
    for k, st in enumerate((0, 5, 10, 15)):
        sh.alpha_composite(Image.open(f"{OUT_RING}/ring_{st:02d}.png"),
                           (k * 80 + 4, 64 * 3 + 8))
    sh.alpha_composite(Image.open(f"{OUT_RING}/arrow_up.png"), (334, 64 * 3 + 20))
    sh.alpha_composite(Image.open(f"{OUT_RING}/arrow_down.png"), (334, 64 * 3 + 52))
    sh.convert("RGB").resize((64 * 10, (64 * 3 + 100) * 2), Image.LANCZOS).save(
        _os.path.join(_HERE, "preview") + "/icons.png")


if __name__ == "__main__":
    main()
