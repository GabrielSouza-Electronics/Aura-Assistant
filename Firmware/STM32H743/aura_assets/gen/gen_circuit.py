#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Tema CIRCUITO VIVO - Aura Assistant.

O fundo e uma placa de circuito vista sob luz fraca; as particulas sao pulsos
que correm PELOS TRACOS, nao a deriva.

A DIFERENCA PARA MATRIX
Chuva de codigo cai de forma uniforme, sem destino, e cobre a tela inteira -
compete com o texto e vira ruido. Aqui os pulsos tem ROTA: saem de um pad,
seguem um traco, param numa via, dobram em 45 graus. Poucos pontos brilhantes
sobre linhas escuras. Le como processamento, nao como estatica.

REGRAS DO DESENHO DE PCB (e o que faz parecer uma placa de verdade)
  - Traços só em 0, 45 e 90 graus. Curva arbitraria denuncia desenho a mao.
  - Vias (furos) nos pontos onde o traço muda de camada.
  - Pads maiores nas extremidades, onde "haveria" um componente.
  - Larguras diferentes: barramentos grossos, sinais finos.
  - Densidade irregular: regioes cheias e regioes vazias, como numa placa real.

Saida:
    circuit/board.png        fundo 480x480, traços estaticos
    circuit/pulse_<n>.png    sprites do pulso, 4 tamanhos
    circuit/via_glow.png     brilho que acende numa via quando o pulso passa
"""

import os, math, random
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

import os as _os
_HERE = _os.path.dirname(_os.path.abspath(__file__))
OUT = _os.path.join(_HERE, "circuit")

W = H = 480
SS = 3
R_SAFE = 232          # nada de traço alem disto: display redondo

# paleta: a placa e escura e fria; os pulsos e' que sao quentes de luz
C_TRACE_LO = (6, 22, 44)
C_TRACE_HI = (26, 74, 122)
C_PULSE_LO = (12, 60, 140)
C_PULSE_MID = (70, 190, 250)
C_PULSE_HI = (238, 252, 255)

random.seed(11)


def colorize(t, lo, mid, hi, boost=1.25, levels=120):
    t = np.clip(t, 0, 1).astype(np.float32)
    t = np.clip(np.round(t * (levels - 1)) / (levels - 1), 0, 1)
    lo, mid, hi = map(np.array, (lo, mid, hi))
    a = np.clip(t / 0.50, 0, 1)[..., None]
    b = np.clip((t - 0.50) / 0.50, 0, 1)[..., None]
    rgb = lo * (1 - a) + mid * a
    rgb = rgb * (1 - b) + hi * b
    return Image.fromarray(
        np.dstack([rgb, np.clip(t * boost, 0, 1) * 255]).astype(np.uint8), "RGBA")


# ---------------------------------------------------------------- rotas
def manhattan45(p0, p1):
    """
    Caminho entre dois pontos usando apenas segmentos de 0, 45 e 90 graus.
    E a regra que define a aparencia de PCB: qualquer angulo livre destroi a
    leitura na hora.
    """
    x0, y0 = p0
    x1, y1 = p1
    dx, dy = x1 - x0, y1 - y0
    adx, ady = abs(dx), abs(dy)
    sx = 1 if dx > 0 else -1
    sy = 1 if dy > 0 else -1

    pts = [(x0, y0)]
    if adx > ady:
        # reto, depois diagonal
        pts.append((x0 + sx * (adx - ady), y0))
    else:
        pts.append((x0, y0 + sy * (ady - adx)))
    pts.append((x1, y1))
    return pts


def build_routes(n=64):
    """Gera as rotas da placa. Cada uma e uma lista de pontos."""
    routes = []
    hubs = []
    # hubs: pontos de onde varias rotas saem, como um chip na placa
    # hubs num anel, nao no centro: o meio da tela pertence a interface, e
    # uma placa real tambem distribui os componentes pela area
    for i in range(10):
        a = 2 * math.pi * i / 10 + random.uniform(-0.25, 0.25)
        r = random.uniform(R_SAFE * 0.42, R_SAFE * 0.82)
        hubs.append((240 + r * math.cos(a), 240 + r * math.sin(a)))

    for i in range(n):
        if random.random() < 0.45 and hubs:
            p0 = random.choice(hubs)
        else:
            a = random.uniform(0, 2 * math.pi)
            r = R_SAFE * 0.95 * math.sqrt(random.random())
            p0 = (240 + r * math.cos(a), 240 + r * math.sin(a))

        # destino perto da origem: rotas longas atravessando o disco inteiro
        # deixam o meio riscado e as bordas vazias
        ang = random.uniform(0, 2 * math.pi)
        dist = random.uniform(70, 190)
        p1 = (p0[0] + dist * math.cos(ang), p0[1] + dist * math.sin(ang))

        pts = manhattan45(p0, p1)
        # descarta rotas muito curtas ou que saem do circulo
        ok = all(math.hypot(x - 240, y - 240) < R_SAFE for x, y in pts)
        L = sum(math.hypot(pts[k + 1][0] - pts[k][0], pts[k + 1][1] - pts[k][1])
                for k in range(len(pts) - 1))
        if ok and L > 60:
            routes.append({
                "pts": pts,
                "w": random.choice([1.0, 1.0, 1.4, 2.0]),   # sinal ou barramento
                "len": L,
            })
    return routes, hubs


def build_board(routes, hubs):
    img = Image.new("L", (W * SS, H * SS), 0)
    dr = ImageDraw.Draw(img)

    for rt in routes:
        p = [(x * SS, y * SS) for x, y in rt["pts"]]
        wd = max(1, int(rt["w"] * SS))
        dr.line(p, fill=86, width=wd, joint="curve")
        # via em cada vertice: o furo onde o traço mudaria de camada
        for x, y in p[1:-1]:
            r = wd * 1.9
            dr.ellipse([x - r, y - r, x + r, y + r], outline=128, width=max(1, SS // 2))
        # pads nas extremidades
        for x, y in (p[0], p[-1]):
            r = wd * 2.6
            dr.ellipse([x - r, y - r, x + r, y + r], outline=150, width=max(1, SS))

    # hubs: retangulo chanfrado, como a pegada de um componente
    for hx, hy in hubs:
        s = random.uniform(9, 16) * SS
        c = s * 0.28
        x, y = hx * SS, hy * SS
        pts = [(x - s + c, y - s), (x + s - c, y - s), (x + s, y - s + c),
               (x + s, y + s - c), (x + s - c, y + s), (x - s + c, y + s),
               (x - s, y + s - c), (x - s, y - s + c)]
        dr.line(pts + [pts[0]], fill=110, width=max(1, SS), joint="curve")

    small = img.resize((W, H), Image.LANCZOS)
    a = np.asarray(small, np.float32) / 255.0
    g = np.asarray(small.filter(ImageFilter.GaussianBlur(1.6)), np.float32) / 255.0
    t = np.clip(a * 0.95 + g * 0.42, 0, 1)

    # o traço e ESCURO: ele e cenario, nao conteudo. Se brilhar, compete com
    # os pulsos e com o texto da interface.
    t *= 0.50

    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    d = np.sqrt((xx - 240) ** 2 + (yy - 240) ** 2)
    t *= np.clip((R_SAFE + 6 - d) / 14.0, 0, 1)

    return colorize(t, C_TRACE_LO, C_TRACE_HI, (60, 130, 190), 1.0, 90)


# ---------------------------------------------------------------- pulsos
PULSE_SIZES = [7, 11, 16, 22]


def build_pulse(size, frame, nframes=8):
    n = size * 6
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float32)
    c = (n - 1) / 2.0
    x = (xx - c) / n
    y = (yy - c) / n
    d = np.sqrt(x * x + y * y)
    s = 0.5 + 0.5 * math.sin(2 * math.pi * frame / nframes)

    core = np.exp(-(d / (0.055 + 0.018 * s)) ** 2)
    halo = np.exp(-(d / (0.20 + 0.05 * s)) ** 2) * (0.40 + 0.18 * s)
    t = np.clip(core * 1.25 + halo, 0, 1)
    t *= np.clip((0.50 - d) / 0.10, 0, 1)

    small = Image.fromarray((t * 255).astype(np.uint8), "L").resize(
        (size, size), Image.LANCZOS)
    a = np.asarray(small, np.float32) / 255.0
    return colorize(a, C_PULSE_LO, C_PULSE_MID, C_PULSE_HI, 1.30, 90)


RING_SIZES = [14, 20, 28]


def build_ringed(size, frame, nframes=8):
    """
    Particula com ANEL concentrico em volta - o mesmo vocabulario das vias da
    placa. Nao e glow: e um contorno nitido, separado do nucleo por um vao
    escuro. E isso que faz o ponto parecer um componente e nao uma luz.

    Parte das particulas leva o anel; ele as acompanha quando convergem para
    a esfera, entao a esfera fica com pontos de dois tipos e ganha relevo.
    """
    n = size * 6
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float32)
    c = (n - 1) / 2.0
    d = np.sqrt((xx - c) ** 2 + (yy - c) ** 2) / n
    s = 0.5 + 0.5 * math.sin(2 * math.pi * frame / nframes)

    # nucleo compacto
    core = np.exp(-(d / 0.055) ** 2) * (0.90 + 0.20 * s)

    # anel: fino e nitido, com um vao escuro separando-o do nucleo
    ring = np.exp(-((d - 0.195) / 0.030) ** 2) * (0.78 + 0.22 * s)

    # segundo anel, mais fraco e mais largo - respira em fase oposta, o que
    # sugere uma onda saindo do ponto
    ring2 = np.exp(-((d - (0.300 + 0.040 * s)) / 0.026) ** 2) * (0.30 - 0.14 * s)

    t = np.clip(core + ring + ring2, 0, 1)
    t *= np.clip((0.50 - d) / 0.08, 0, 1)

    small = Image.fromarray((t * 255).astype(np.uint8), "L").resize(
        (size, size), Image.LANCZOS)
    return colorize(np.asarray(small, np.float32) / 255.0,
                    C_PULSE_LO, C_PULSE_MID, C_PULSE_HI, 1.26, 100)


def build_via_glow(size=26):
    n = size * 6
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float32)
    c = (n - 1) / 2.0
    d = np.sqrt((xx - c) ** 2 + (yy - c) ** 2) / n
    ring = np.exp(-((d - 0.17) / 0.045) ** 2)
    glow = np.exp(-(d / 0.26) ** 2) * 0.45
    t = np.clip(ring * 0.9 + glow, 0, 1)
    t *= np.clip((0.50 - d) / 0.10, 0, 1)
    small = Image.fromarray((t * 255).astype(np.uint8), "L").resize(
        (size, size), Image.LANCZOS)
    return colorize(np.asarray(small, np.float32) / 255.0,
                    C_PULSE_LO, C_PULSE_MID, C_PULSE_HI, 1.2, 80)


def main():
    os.makedirs(OUT, exist_ok=True)
    os.makedirs(_os.path.join(_HERE, "preview"), exist_ok=True)

    routes, hubs = build_routes()
    board = build_board(routes, hubs)
    board.save(f"{OUT}/board.png")

    # grava as rotas para o mockup animar os pulsos sobre elas
    import json
    json.dump([{"pts": r["pts"], "w": r["w"], "len": r["len"]} for r in routes],
              open(f"{OUT}/routes.json", "w"))

    total = W * H + 1024
    worst = len(np.unique(np.asarray(board).reshape(-1, 4), axis=0))
    for sz in PULSE_SIZES:
        for f in range(8):
            im = build_pulse(sz, f)
            im.save(f"{OUT}/pulse_{sz}_{f}.png")
            total += sz * sz + 1024
            worst = max(worst, len(np.unique(np.asarray(im).reshape(-1, 4), axis=0)))
    for sz in RING_SIZES:
        for f in range(8):
            im = build_ringed(sz, f)
            im.save(f"{OUT}/ringed_{sz}_{f}.png")
            total += sz * sz + 1024
            worst = max(worst, len(np.unique(np.asarray(im).reshape(-1, 4), axis=0)))

    vg = build_via_glow()
    vg.save(f"{OUT}/via_glow.png")
    total += 26 * 26 + 1024

    print(f"  placa {W}x{H}, {len(routes)} rotas, {len(hubs)} hubs")
    print(f"  pulsos: {len(PULSE_SIZES)} tamanhos x 8 frames")
    print(f"  com anel: {len(RING_SIZES)} tamanhos x 8 frames")
    print(f"  pior paleta: {worst} RGBA  {'L8 ok' if worst <= 256 else 'ESTOUROU'}")
    print(f"  flash: {total/1024:.0f} KB em L8_ARGB8888")

    bg = Image.new("RGBA", (W, H), (3, 8, 16, 255))
    bg.alpha_composite(board)
    bg.convert("RGB").save(_os.path.join(_HERE, "preview") + "/board.png")


if __name__ == "__main__":
    main()
