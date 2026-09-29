#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Rotaciona todos os assets em 270 graus - Aura Assistant.

O display esta montado girado, entao tudo que aparece precisa estar girado
junto: os PNGs E as coordenadas.

A TRANSFORMACAO
Rotacao de 270 graus (90 anti-horario) num sistema de 480x480:

    (x, y)  ->  (y, 479 - x)

Um sprite de w x h vira h x w. Para um asset CENTRADO num ponto, basta
rotacionar a imagem e transformar o centro - o codigo continua posicionando
pelo centro e nada mais muda.

Para assets ANCORADOS pelo canto (o fundo da placa, a borda), a origem
tambem se desloca, e por isso o script devolve os dois valores.

    python gen_rotate.py            # gera em rot/
    python gen_rotate.py --check    # so imprime a tabela de coordenadas
"""

import os, sys, shutil
from PIL import Image

import os as _os
_HERE = _os.path.dirname(_os.path.abspath(__file__))
SRC_DIRS = ["circuit", "icons", "ring", "text", "logo", "status",
            "divider", "hero", "rim"]
OUT = _os.path.join(_HERE, "rot")

W = H = 480


def rot_point(x, y):
    """Centro de um elemento, apos girar a tela 270 graus."""
    return (y, (H - 1) - x)


def rot_topleft(x, y, w, h):
    """
    Canto superior esquerdo apos a rotacao.

    Uso H (480) e nao H-1 aqui: o canto e uma FRONTEIRA entre pixels, nao um
    pixel. Com 479 o resultado saia deslocado em 1 px e um asset de tela
    cheia ficava em y=-1.
    """
    return (y, H - (x + w))


# posicoes originais de tudo o que e' fixo na tela
LAYOUT = {
    "board":        (0, 0, 480, 480),
    "rim":          (0, 0, 480, 480),
    "logo_status":  (218, 20, 44, 44),
    "wifi":         (184, 32, 22, 22),
    "battery":      (274, 35, 30, 15),
    "hero":         (140, 78, 200, 200),
    "lbl_carousel": (None, 372, None, 29),     # centrado em x=240
    "lbl_menu":     (None, 236, None, 29),
    "divider_line": (110, 318, 260, 9),
    "divider_spark": (227, 309, 26, 26),
    "msg_state":    (None, 418, None, 17),
}

# centros de referencia usados pelo codigo
CENTERS = {
    "tela":            (240, 240),
    "esfera":          (240, 236),
    "carrossel":       (240, 232),
    "logo hero":       (240, 178),
    "logo status":     (240, 42),
    "rotulo carrossel": (240, 372),
    "rotulo menu":     (240, 236),
}


def rotate_all():
    if _os.path.isdir(OUT):
        shutil.rmtree(OUT)
    os.makedirs(OUT)

    n = 0
    for d in SRC_DIRS:
        src = _os.path.join(_HERE, d)
        if not _os.path.isdir(src):
            continue
        for root, _, files in os.walk(src):
            for f in files:
                if not f.endswith(".png"):
                    continue
                p = _os.path.join(root, f)
                rel = _os.path.relpath(p, _HERE)
                dst = _os.path.join(OUT, rel)
                os.makedirs(_os.path.dirname(dst), exist_ok=True)
                # ROTATE_90 do PIL gira no sentido ANTI-HORARIO, que e o
                # que "270 graus" quer dizer aqui. expand=True porque
                # sprites nao quadrados trocam largura e altura.
                Image.open(p).rotate(90, expand=True).save(dst)
                n += 1
    return n


def print_table():
    print("CENTROS — use estes valores no codigo\n")
    print(f"  {'elemento':20s} {'antes':>12s}  {'depois':>12s}")
    print("  " + "-" * 48)
    for k, (x, y) in CENTERS.items():
        nx, ny = rot_point(x, y)
        print(f"  {k:20s} {f'({x},{y})':>12s}  {f'({nx},{ny})':>12s}")

    print("\n\nCANTO SUPERIOR ESQUERDO — para assets ancorados pelo canto\n")
    print(f"  {'asset':16s} {'antes':>16s} {'tam':>10s}  {'depois':>12s} {'tam':>10s}")
    print("  " + "-" * 70)
    for k, (x, y, w, h) in LAYOUT.items():
        if x is None:
            print(f"  {k:16s} {'centrado em x':>16s} {'':>10s}  "
                  f"{'centrado em y':>12s}")
            continue
        nx, ny = rot_topleft(x, y, w, h)
        print(f"  {k:16s} {f'({x},{y})':>16s} {f'{w}x{h}':>10s}  "
              f"{f'({nx},{ny})':>12s} {f'{h}x{w}':>10s}")

    print("\n\nO QUE MUDA NO CODIGO DAS PARTICULAS\n")
    print("  A esfera e o carrossel sao calculados em coordenadas de tela.")
    print("  Em vez de transformar cada particula, gire o SISTEMA: troque as")
    print("  duas linhas finais da projecao por")
    print()
    print("      sx = SPH_CY - ty * k          /* era CX + x1 * k   */")
    print("      sy = (479 - CX) + x1 * k      /* era SPH_CY - ty*k */")
    print()
    print("  Uma transformacao no fim da projecao custa duas operacoes por")
    print("  particula e mantem toda a matematica de orbita intacta. Rodar")
    print("  cada sprite em tempo de execucao custaria muito mais.")


if __name__ == "__main__":
    if "--check" in sys.argv:
        print_table()
    else:
        n = rotate_all()
        print(f"  {n} assets rotacionados 270 graus -> rot/\n")
        print_table()
