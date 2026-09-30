#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Menu SETTINGS - Aura Assistant.

Quatro linhas translucidas (Wi-Fi, Bluetooth, Brightness, Sound) que
flutuam sobre o campo de particulas. O fundo continua vivo:
o painel tem ~40% de opacidade, entao as bolhas e a esfera aparecem atraves
dele.

DECISOES
  - Cada linha e UM sprite: caixa + icone + rotulo + chevron. Sao fixos,
    entao assar tudo junto troca 4 widgets por 1. O VALOR e texto livre,
    editado em tempo de execucao: sai de um ATLAS DE GLIFOS (glyphs/g_XX,
    XX = codigo ASCII em hex) desenhado pelo widget GlyphText. TextArea nao
    serve: nao acompanha o Layout Rotation.
  - Duas versoes por linha: normal e em foco (borda e barra acesas). Um
    halo separado (glow_<i>) fica atras da linha em foco e "respira".
  - As caixas acompanham a curva do display: a largura de cada uma e
    limitada pelo circulo util, mas o conteudo (icone, texto, valor,
    chevron) fica em colunas fixas - alinhado como lista, recortado como
    lente.
  - Cantos chanfrados assimetricos, mesmo vocabulario dos icones
    (gen_icons.py): corte grande em dois cantos, pequeno nos outros, e um
    gap na borda superior.
  - Fonte, tracking e rampa de cor vem do gen_text.py - as palavras novas
    tem que parecer escritas pela mesma mao das antigas.
  - Toda imagem sai quantizada em <= 256 cores RGBA (L8_ARGB8888) e com
    alpha 0 na borda.

Tudo e desenhado em coordenadas LOGICAS (nao giradas) e girado 90 graus
anti-horario na saida, igual ao gen_rotate.py.

Saida:
    gen/settings/                 PNGs nao girados (referencia)
    assets_rotacionados/settings/ PNGs girados
    TouchGFX/assets/images/aura/settings/   (com --install)
    assets_rotacionados/glyphs/   atlas de glifos dos valores, girado
    TouchGFX/assets/images/aura/glyphs/     (com --install)
    TouchGFX/gui/include/gui/common/SettingsLayout.hpp   (com --install)
    TouchGFX/gui/include/gui/common/SettingsGlyphs.hpp   (com --install)
    preview/settings_menu.png, preview/settings_menu.gif

    python gen_settings.py            # gera e faz o preview
    python gen_settings.py --install  # tambem copia para o TouchGFX
"""

import os, sys, math, shutil, random
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

import gen_text as gt
import gen_icons as gi

HERE = os.path.dirname(os.path.abspath(__file__))
BASE = os.path.dirname(HERE)                       # aura_assets/
OUT = os.path.join(HERE, "settings")
OUT_ROT = os.path.join(BASE, "assets_rotacionados", "settings")
PREVIEW = os.path.join(BASE, "preview")
TGFX = os.path.join(os.path.dirname(BASE), "TouchGFX")
TGFX_IMG = os.path.join(TGFX, "assets", "images", "aura", "settings")
OUT_GLYPH = os.path.join(BASE, "assets_rotacionados", "glyphs")
TGFX_GLYPH = os.path.join(TGFX, "assets", "images", "aura", "glyphs")
LAYOUT_HPP = os.path.join(TGFX, "gui", "include", "gui", "common",
                          "SettingsLayout.hpp")
GLYPH_HPP = os.path.join(TGFX, "gui", "include", "gui", "common",
                         "SettingsGlyphs.hpp")

SS = 6
W = H = 480

# ------------------------------------------------------------------ layout
# Coordenadas LOGICAS (como a pessoa ve o display), y para baixo.
TITLE_CY = 88           # lbl_settings reaproveitado
SUB_CY = 113
ROW_Y0 = 136
ROW_H = 46
ROW_GAP = 8
ROW_PITCH = ROW_H + ROW_GAP
R_SAFE = 214            # raio onde os cantos das caixas ainda cabem
HW_MAX = 178            # meia-largura maxima de uma caixa
ICON_CX = 104
ICON_N = 30
LABEL_X = 130           # inicio do texto visivel
VALUE_RX = 352          # fim do texto visivel do valor
CHEV_CX = 377
PAD = 4                 # margem transparente do sprite da linha
GLOW_PAD = 18

ROWS = [
    ("wifi",       "WI-FI"),
    ("bluetooth",  "BLUETOOTH"),
    ("brightness", "BRIGHTNESS"),
    ("sound",      "SOUND"),
]

# Atlas dos valores. O texto e convertido para MAIUSCULAS no GlyphText
# (mesmo estilo dos rotulos); caracteres fora do atlas viram '?'.
GLYPH_PT = 18
GLYPH_PAD = 3           # margem de cada celula, px logicos
GLYPH_CHARS = ("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
               "-_.,:;/%+&'()!?#@*")
# valores de exemplo so para o preview
PREVIEW_VALUES = ["HOMENET", "ON", "70%", "5/10"]
SUBTITLE = "CUSTOMIZE YOUR AURA"

C_CYAN = np.array((78, 196, 252), np.float32)
C_FILL_TOP = np.array((10, 30, 56), np.float32)
C_FILL_BOT = np.array((5, 16, 34), np.float32)
C_FOCUS = np.array((40, 220, 150), np.float32)
C_FOCUS_ACCENT = np.array((184, 255, 219), np.float32)
C_FOCUS_TOP = np.array((8, 65, 43), np.float32)
C_FOCUS_BOT = np.array((4, 32, 23), np.float32)

# rampa do valor: mais azul e menos branca que a do rotulo - o valor e
# informacao secundaria, como na referencia.
VAL_RAMP = ((184, 223, 243),) * 3


def row_rect(i):
    """(x0, y0, x1, y1) logico da caixa i."""
    y0 = ROW_Y0 + i * ROW_PITCH
    y1 = y0 + ROW_H
    dyf = max(abs(y0 - 240), abs(y1 - 240))
    hw = int(min(HW_MAX, math.sqrt(R_SAFE * R_SAFE - dyf * dyf)))
    return (240 - hw, y0, 240 + hw, y1)


# ------------------------------------------------------------------ icones
# Mesmo grid de 24 e mesma hierarquia (primario 255, secundario DIM) do
# gen_icons.py.
G = gi.G
DIM = int(255 * gi.DIM)


def draw_wifi(dr, u, w):
    cx, cy = 12 * G, 18.5 * G
    for k, r in enumerate((4.5, 8.5, 12.5)):
        v = 255 if k < 2 else DIM
        dr.arc([u(cx - r * G), u(cy - r * G), u(cx + r * G), u(cy + r * G)],
               225, 315, fill=v, width=w)
    r = 1.4 * G
    dr.ellipse([u(cx - r), u(cy - r), u(cx + r), u(cy + r)], fill=255)


def draw_bluetooth(dr, u, w):
    pts = [(6.5, 7.5), (17, 16.5), (12, 21), (12, 3), (17, 7.5), (6.5, 16.5)]
    dr.line([(u(x * G), u(y * G)) for x, y in pts], fill=255, width=w,
            joint="curve")
    for sg in (-1, 1):           # pontos laterais: detalhe secundario
        r = 0.9 * G
        x, y = 12 * G + sg * 8.5 * G, 12 * G
        dr.ellipse([u(x - r), u(y - r), u(x + r), u(y + r)], fill=DIM)


def draw_brightness(dr, u, w):
    c = 12 * G
    r = 4.2 * G
    dr.ellipse([u(c - r), u(c - r), u(c + r), u(c + r)], outline=255, width=w)
    for i in range(8):
        a = math.pi / 4 * i
        r0, r1 = (6.8, 9.8) if i % 2 == 0 else (7.0, 8.6)
        dr.line([u(c + r0 * G * math.cos(a)), u(c + r0 * G * math.sin(a)),
                 u(c + r1 * G * math.cos(a)), u(c + r1 * G * math.sin(a))],
                fill=255 if i % 2 == 0 else DIM, width=w)


def draw_sound(dr, u, w):
    body = [(3.5, 9.5), (7.5, 9.5), (12.5, 5), (12.5, 19), (7.5, 14.5),
            (3.5, 14.5), (3.5, 9.5)]
    dr.line([(u(x * G), u(y * G)) for x, y in body], fill=255, width=w,
            joint="curve")
    for k, r in enumerate((4.0, 7.5)):
        dr.arc([u((12 - r) * G), u((12 - r) * G), u((12 + r) * G),
                u((12 + r) * G)], -48, 48, fill=255 if k == 0 else DIM, width=w)


DRAW = {"wifi": draw_wifi, "bluetooth": draw_bluetooth,
        "brightness": draw_brightness, "sound": draw_sound}


def build_icon(name, n=ICON_N):
    gi.DRAW[name] = DRAW[name]
    return gi.build_icon(name, n)


def build_chevron(bright):
    n = 14
    img = Image.new("L", (n * SS, n * SS), 0)
    dr = ImageDraw.Draw(img)
    s = n * SS
    w = max(1, int(round(0.11 * s)))
    dr.line([(s * 0.34, s * 0.18), (s * 0.66, s * 0.5), (s * 0.34, s * 0.82)],
            fill=255, width=w, joint="curve")
    small = img.resize((n, n), Image.LANCZOS)
    base = np.asarray(small, np.float32) / 255.0
    g1 = np.asarray(small.filter(ImageFilter.GaussianBlur(1.0)), np.float32) / 255.0
    t = np.clip(base * 1.05 + g1 * (0.45 if bright else 0.2), 0, 1)
    t *= 1.0 if bright else 0.55
    return gi.colorize(t)


# ------------------------------------------------------------------ caixa
def chamfer_poly(x0, y0, x1, y1, big, small):
    """Chanfro grande em cima-esquerda e baixo-direita, pequeno nos outros."""
    return [(x0 + big, y0), (x1 - small, y0), (x1, y0 + small),
            (x1, y1 - big), (x1 - big, y1), (x0 + small, y1),
            (x0, y1 - small), (x0, y0 + big)]


def panel_layers(w, h, focused):
    """Devolve (fill_mask, border_mask, accent_mask) em float 0..1, w x h."""
    S = SS
    big, small = 10, 3
    poly = [(x * S, y * S) for x, y in
            chamfer_poly(PAD, PAD, w - PAD - 1, h - PAD - 1, big, small)]

    fill = Image.new("L", (w * S, h * S), 0)
    ImageDraw.Draw(fill).polygon(poly, fill=255)

    border = Image.new("L", (w * S, h * S), 0)
    bd = ImageDraw.Draw(border)
    bw = int(1.2 * S) if not focused else int(1.6 * S)
    bd.line(poly + [poly[0]], fill=255, width=bw, joint="curve")
    # gap HUD na borda de cima, perto da direita
    gx0 = (w - PAD - 1 - 64) * S
    bd.rectangle([gx0, 0, gx0 + 16 * S, (PAD + 2) * S], fill=0)
    # marcas de canto: dois tiques curtos fora do chanfro grande
    tick = 255
    bd.line([((PAD + 3) * S, (PAD + big + 5) * S), ((PAD + 3) * S, (PAD + big + 11) * S)],
            fill=tick, width=bw)

    accent = Image.new("L", (w * S, h * S), 0)
    ad = ImageDraw.Draw(accent)
    ax = (PAD + 7) * S
    ad.rectangle([ax, (PAD + 13) * S, ax + int(2.2 * S), (h - PAD - 13) * S], fill=255)

    def down(im):
        return np.asarray(im.resize((w, h), Image.LANCZOS), np.float32) / 255.0

    return down(fill), down(border), down(accent)


def build_row(i, focused):
    x0, y0, x1, y1 = row_rect(i)
    w = x1 - x0 + 2 * PAD
    h = y1 - y0 + 2 * PAD
    fill, border, accent = panel_layers(w, h, focused)

    # vidro: gradiente vertical no preenchimento, mais claro em cima
    ty = np.linspace(0, 1, h, dtype=np.float32)[:, None, None]
    top, bot = (C_FOCUS_TOP, C_FOCUS_BOT) if focused else (C_FILL_TOP, C_FILL_BOT)
    fill_rgb = top * (1 - ty) + bot * ty
    fill_a = (0.60 - 0.14 * ty[..., 0]) if focused else (0.46 - 0.12 * ty[..., 0])
    fill_a = fill * fill_a

    border_a = border * (0.95 if focused else 0.45)
    accent_g = np.asarray(Image.fromarray((accent * 255).astype(np.uint8))
                          .filter(ImageFilter.GaussianBlur(2.2)), np.float32) / 255.0
    accent_a = np.clip(accent * (1.0 if focused else 0.50)
                       + accent_g * (0.9 if focused else 0.0), 0, 1)

    # composicao "over": fill -> borda -> barra
    rgb = np.broadcast_to(fill_rgb, (h, w, 3)).copy()
    a = fill_a.copy()
    border_color = C_FOCUS if focused else C_CYAN
    accent_color = C_FOCUS_ACCENT if focused else np.array((200, 244, 255), np.float32)
    for col, la in ((border_color, border_a), (accent_color, accent_a)):
        la3 = la[..., None]
        out_a = la + a * (1 - la)
        rgb = np.where(out_a[..., None] > 0,
                       (col * la3 + rgb * a[..., None] * (1 - la3))
                       / np.maximum(out_a[..., None], 1e-6), rgb)
        a = out_a
    panel = Image.fromarray(np.dstack([rgb, a * 255]).clip(0, 255).astype(np.uint8), "RGBA")

    # conteudo em colunas fixas (coordenadas relativas ao sprite)
    ox = PAD - x0
    oy = PAD - y0
    cy = (y0 + y1) / 2.0

    key, label = ROWS[i]
    ic = build_icon(key)
    if not focused:
        ic = fade(ic, 0.82)
    panel.alpha_composite(ic, (int(ICON_CX - ICON_N / 2 + ox), int(round(cy - ICON_N / 2 + oy))))

    lb = gt.render_text(label, 18, gt.FONT_MED)
    if not focused:
        lb = fade(lb, 0.88)
    panel.alpha_composite(lb, (int(LABEL_X - 6 + ox), int(round(cy - lb.height / 2 + oy))))

    ch = build_chevron(focused)
    panel.alpha_composite(ch, (int(CHEV_CX - ch.width / 2 + ox), int(round(cy - ch.height / 2 + oy))))
    return panel


def build_glow(i):
    """Halo da linha em foco: so o contorno borrado, o miolo fica quase
    vazio para nao lavar o texto."""
    x0, y0, x1, y1 = row_rect(i)
    w = x1 - x0 + 2 * GLOW_PAD
    h = y1 - y0 + 2 * GLOW_PAD
    S = SS
    m = Image.new("L", (w * S, h * S), 0)
    poly = [(x * S, y * S) for x, y in
            chamfer_poly(GLOW_PAD, GLOW_PAD, w - GLOW_PAD - 1, h - GLOW_PAD - 1, 10, 3)]
    ImageDraw.Draw(m).line(poly + [poly[0]], fill=255, width=3 * S, joint="curve")
    m = m.resize((w, h), Image.LANCZOS)
    g1 = np.asarray(m.filter(ImageFilter.GaussianBlur(2.5)), np.float32) / 255.0
    g2 = np.asarray(m.filter(ImageFilter.GaussianBlur(6.0)), np.float32) / 255.0
    t = np.clip(g1 * 0.55 + g2 * 1.1, 0, 1)
    # corta a cauda do blur: sem isso a borda do sprite fica com alpha > 0
    t = np.clip((t - 0.03) / 0.97, 0, 1)
    # Emerald focus halo; keep the blue palette on unselected rows.
    lo, hi = np.array((6, 90, 53), np.float32), np.array((48, 225, 153), np.float32)
    rgb = lo * (1 - t[..., None]) + hi * t[..., None]
    return Image.fromarray(np.dstack([rgb, t * 0.85 * 255]).astype(np.uint8), "RGBA")


def render_glyph(c):
    """Um caractere, com o mesmo acabamento do gen_text.render_text.
    Devolve (imagem, avanco em px logicos). O avanco inclui o tracking."""
    pt = GLYPH_PT
    f = ImageFont.truetype(gt.FONT_MED, pt * SS)
    probe = ImageDraw.Draw(Image.new("L", (8, 8)))
    adv = (probe.textlength(c, font=f) + gt.TRACK * pt * SS) / SS
    cw = int(math.ceil(adv)) + 2 * GLYPH_PAD
    # 2 px a mais em cima e embaixo: parenteses passam da altura do render_text
    h = int(pt * SS * 1.45) + 4 * SS
    img = Image.new("L", (cw * SS, h), 0)
    ImageDraw.Draw(img).text((GLYPH_PAD * SS, h / 2), c, font=f, fill=255,
                             anchor="lm")
    saved = (gt.C_LOW, gt.C_MID, gt.C_HIGH)
    gt.C_LOW, gt.C_MID, gt.C_HIGH = VAL_RAMP
    try:
        return gt.finish(img), adv
    finally:
        gt.C_LOW, gt.C_MID, gt.C_HIGH = saved


def build_glyphs():
    """codigo ASCII -> (imagem, avanco). O espaco so tem avanco."""
    out = {ord(c): render_glyph(c) for c in GLYPH_CHARS}
    f = ImageFont.truetype(gt.FONT_MED, GLYPH_PT * SS)
    probe = ImageDraw.Draw(Image.new("L", (8, 8)))
    space = (probe.textlength(" ", font=f) + gt.TRACK * GLYPH_PT * SS) / SS
    return {k: (quant256(im), a) for k, (im, a) in out.items()}, space


def glyph_string(glyphs, space, text):
    """Monta um valor com o atlas - mesma regra do GlyphText.cpp."""
    cells, x = [], 0.0
    for ch in text.upper():
        if ch == " ":
            x += space
            continue
        im, adv = glyphs.get(ord(ch), glyphs[ord("?")])
        cells.append((im, int(round(x))))
        x += adv
    ext = max([p + im.width for im, p in cells] + [1])
    out = Image.new("RGBA", (ext, next(iter(glyphs.values()))[0].height), 0)
    for im, p in cells:
        out.alpha_composite(im, (p, 0))
    return out


def fade(im, k):
    a = np.asarray(im).copy()
    a[..., 3] = (a[..., 3].astype(np.float32) * k).astype(np.uint8)
    return Image.fromarray(a, "RGBA")


# ------------------------------------------------------------------ saida
def quant256(im, preserve_clear=False):
    """Garante <= 256 cores RGBA sem dithering (regra do L8)."""
    q = im.quantize(colors=255 if preserve_clear else 256, method=Image.Quantize.FASTOCTREE,
                    dither=Image.Dither.NONE).convert("RGBA")
    a = np.asarray(q).copy()
    if preserve_clear:
        # Reserve one palette entry for exact transparency; quantization can
        # otherwise merge the clear halo edge with low-alpha pixels.
        a[np.asarray(im)[..., 3] == 0] = 0
    a[a[..., 3] == 0] = 0                 # transparente e sempre (0,0,0,0)
    return Image.fromarray(a, "RGBA")


def ncolors(im):
    return len(np.unique(np.asarray(im).reshape(-1, 4), axis=0))


def edge_alpha(im):
    a = np.asarray(im)[..., 3]
    return int(max(a[0].max(), a[-1].max(), a[:, 0].max(), a[:, -1].max()))


def build_all():
    """nome -> (imagem logica, (x, y) logico do canto ou None)"""
    out = {}
    for i in range(len(ROWS)):
        x0, y0, _, _ = row_rect(i)
        out[f"row_{i}"] = (build_row(i, False), (x0 - PAD, y0 - PAD))
        out[f"rowf_{i}"] = (build_row(i, True), (x0 - PAD, y0 - PAD))
        out[f"glow_{i}"] = (build_glow(i), (x0 - GLOW_PAD, y0 - GLOW_PAD))
    out["sub_title"] = (gt.render_text(SUBTITLE, 12, gt.FONT_MED), None)
    return {k: (quant256(im, preserve_clear=k.startswith("glow_")), pos)
            for k, (im, pos) in out.items()}


def write_layout(assets):
    """Header com os retangulos JA GIRADOS: (x, y) = (ly, 480 - (lx + lw))."""
    def fb(lx, ly, lw, lh):
        return (ly, W - (lx + lw), lh, lw)

    L = []
    L.append("/* GERADO por aura_assets/gen/gen_settings.py - nao edite a mao. */")
    L.append("#ifndef SETTINGSLAYOUT_HPP")
    L.append("#define SETTINGSLAYOUT_HPP")
    L.append("")
    L.append("#include <stdint.h>")
    L.append("")
    L.append("/* Retangulos em coordenadas de FRAMEBUFFER (ja girados). A tela")
    L.append("   logica gira 90 graus: x logico -> 479 - y, y logico -> x.   */")
    L.append("struct SettingsRect { int16_t x, y, w, h; };")
    L.append("")
    L.append(f"#define SL_ROWS {len(ROWS)}")
    L.append("")
    L.append("/* linhas (caixa + icone + rotulo + chevron) e halo do foco */")
    L.append("static const SettingsRect SL_ROW[SL_ROWS] = {")
    for i in range(len(ROWS)):
        im, (lx, ly) = assets[f"row_{i}"]
        L.append("    {{ {}, {}, {}, {} }},".format(*fb(lx, ly, im.width, im.height)))
    L.append("};")
    L.append("static const SettingsRect SL_GLOW[SL_ROWS] = {")
    for i in range(len(ROWS)):
        im, (lx, ly) = assets[f"glow_{i}"]
        L.append("    {{ {}, {}, {}, {} }},".format(*fb(lx, ly, im.width, im.height)))
    L.append("};")
    L.append("")
    L.append("/* centro logico (y) de cada linha - usado pelo foco da mao */")
    L.append("static const int16_t SL_ROW_CY_LOGICAL[SL_ROWS] = { "
             + ", ".join(str(ROW_Y0 + i * ROW_PITCH + ROW_H // 2) for i in range(len(ROWS)))
             + " };")
    L.append(f"static const int16_t SL_ROW_PITCH = {ROW_PITCH};")
    L.append("")
    L.append("/* valores (GlyphText): a borda direita LOGICA da caixa do texto")
    L.append("   fica nesta coordenada y do framebuffer (inclui a margem da")
    L.append("   celula do glifo); o texto nao passa da largura logica maxima */")
    L.append(f"static const int16_t SL_VALUE_END_FB_Y = {W - (VALUE_RX + GLYPH_PAD)};")
    maxw = []
    for key, label in ROWS:
        ink = gt.render_text(label, 18, gt.FONT_MED).width - 12
        maxw.append(VALUE_RX - (LABEL_X + ink) - 14)
    L.append("static const int16_t SL_VALUE_MAX_W[SL_ROWS] = { "
             + ", ".join(str(v) for v in maxw) + " };")
    L.append("")
    sub, _ = assets["sub_title"]
    L.append(f"static const int16_t SL_SUB_W = {sub.height}, SL_SUB_H = {sub.width};")
    L.append(f"static const int16_t SL_TITLE_CX_FB = {TITLE_CY};   /* centro x (fb) do titulo */")
    L.append(f"static const int16_t SL_SUB_CX_FB = {SUB_CY};")
    L.append("")
    L.append("#endif")
    with open(LAYOUT_HPP, "w", newline="\n") as f:
        f.write("\n".join(L) + "\n")


def write_glyphs(glyphs, space):
    L = []
    L.append("/* GERADO por aura_assets/gen/gen_settings.py - nao edite a mao. */")
    L.append("#ifndef SETTINGSGLYPHS_HPP")
    L.append("#define SETTINGSGLYPHS_HPP")
    L.append("")
    L.append("#include <stdint.h>")
    L.append("#include <images/BitmapDatabase.hpp>")
    L.append("")
    L.append("/* Atlas de glifos dos valores do menu Settings (Poppins Medium")
    L.append(f"   {GLYPH_PT} pt, tracking do gen_text.py). Celulas ja giradas: no")
    L.append("   framebuffer a largura e a altura do texto e a altura e a")
    L.append("   largura LOGICA da celula. Avancos em 1/16 px logico.       */")
    L.append("struct SettingsGlyph { char c; uint16_t id; uint8_t cellW; uint16_t adv16; };")
    L.append("")
    first = next(iter(glyphs.values()))[0]
    L.append(f"static const int16_t SG_TEXT_H = {first.height};   /* largura no fb */")
    L.append(f"static const int16_t SG_CELL_PAD = {GLYPH_PAD};")
    L.append(f"static const uint8_t SG_SPACE_ADV16 = {int(round(space * 16))};")
    L.append("")
    L.append("/* ordenado por codigo ASCII */")
    L.append(f"static const SettingsGlyph SG_GLYPHS[{len(glyphs)}] = {{")
    for code in sorted(glyphs):
        im, adv = glyphs[code]
        ch = chr(code)
        lit = "'\\''" if ch == "'" else f"'{ch}'"
        L.append(f"    {{ {lit}, BITMAP_G_{code:02X}_ID, {im.width}, {int(round(adv * 16))} }},")
    L.append("};")
    L.append("")
    L.append("#endif")
    with open(GLYPH_HPP, "w", newline="\n") as f:
        f.write("\n".join(L) + "\n")


# ------------------------------------------------------------------ preview
# Replica da SettingsLogic.cpp para o GIF: mesmas constantes de tempo.
REVEAL_DELAY, REVEAL_DUR, SLIDE_PX = 4, 16, 44


def ease(t):
    t = min(1.0, max(0.0, t))
    return t * t * (3 - 2 * t)


def unrot(path):
    return Image.open(path).convert("RGBA").rotate(-90, expand=True)


def make_background(frame):
    """Placa + particulas + borda, em coordenadas logicas."""
    rot = os.path.join(BASE, "assets_rotacionados")
    bg = Image.new("RGBA", (W, H), (3, 8, 16, 255))
    bg.alpha_composite(unrot(os.path.join(rot, "circuit", "board.png")))
    rnd = random.Random(7)
    sprites = [unrot(os.path.join(rot, "circuit", f"{k}_{s}_0.png"))
               for k, s in (("pulse", 7), ("pulse", 11), ("pulse", 16),
                            ("ringed", 14), ("ringed", 20))]
    for n in range(110):
        th = rnd.uniform(0, 2 * math.pi) + frame * 0.02
        ph = math.acos(rnd.uniform(-1, 1))
        r = 108
        x = 240 + r * math.sin(ph) * math.cos(th)
        y = 236 + r * math.cos(ph)
        sp = sprites[rnd.randrange(len(sprites))]
        bg.alpha_composite(sp, (int(x - sp.width / 2), int(y - sp.height / 2)))
    for n in range(40):
        x = rnd.uniform(40, 440) + (frame * 1.5 if n % 2 else -frame) % 60
        y = rnd.uniform(40, 440)
        sp = sprites[rnd.randrange(3)]
        bg.alpha_composite(sp, (int(x), int(y)))
    rim = unrot(os.path.join(rot, "rim", "rim.png"))
    bg.alpha_composite(fade(rim, 140 / 255))
    mask = Image.new("L", (W, H), 0)
    ImageDraw.Draw(mask).ellipse([0, 0, W - 1, H - 1], fill=255)
    out = Image.new("RGBA", (W, H), (0, 0, 0, 255))
    out.paste(bg, (0, 0), mask)
    return out


def compose(assets, frame, t, focus, glow_a, lighten=0, glyphs=None):
    im = make_background(frame)
    if lighten:
        im = Image.blend(im, Image.new("RGBA", (W, H), (lighten,) * 3 + (255,)), 0.12)
    rot = os.path.join(BASE, "assets_rotacionados")
    head = ease(t / 14.0)
    title = unrot(os.path.join(rot, "text", "lbl_settings.png"))
    ty = 236 + (TITLE_CY - 236) * head
    im.alpha_composite(title, (int(240 - title.width / 2), int(ty - title.height / 2)))
    sub = fade(assets["sub_title"][0], 0.8 * head)
    im.alpha_composite(sub, (int(240 - sub.width / 2), int(SUB_CY - sub.height / 2)))

    vals = [glyph_string(glyphs[0], glyphs[1], v) for v in PREVIEW_VALUES]
    for i in range(len(ROWS)):
        r = ease((t - REVEAL_DELAY * i) / REVEAL_DUR)
        if r <= 0:
            continue
        dx = int((1 - r) * SLIDE_PX * (1 if i % 2 == 0 else -1))
        if i == focus:
            g, (gx, gy) = assets[f"glow_{i}"]
            im.alpha_composite(fade(g, r * glow_a / 255), (gx + dx, gy))
        row, (rx, ry) = assets[f"{'rowf' if i == focus else 'row'}_{i}"]
        im.alpha_composite(fade(row, r), (rx + dx, ry))
        v = vals[i]
        cy = ROW_Y0 + i * ROW_PITCH + ROW_H / 2
        im.alpha_composite(fade(v, r), (int(VALUE_RX + GLYPH_PAD - v.width + dx), int(cy - v.height / 2)))
    back = unrot(os.path.join(rot, "text", "msg_back.png"))
    im.alpha_composite(fade(back, 160 / 255), (int(240 - back.width / 2), 410))
    return im


def previews(assets, glyphs):
    os.makedirs(PREVIEW, exist_ok=True)
    still = compose(assets, 0, 60, 2, 230, glyphs=glyphs)
    still.convert("RGB").save(os.path.join(PREVIEW, "settings_menu.png"))
    compose(assets, 0, 60, 2, 230, lighten=60, glyphs=glyphs).convert("RGB").save(
        os.path.join(PREVIEW, "settings_menu_leak.png"))
    frames = []
    focus_path = [-1] * 40 + [0] * 12 + [1] * 12 + [2] * 12 + [3] * 12
    for f, fo in enumerate(focus_path):
        glow = 150 + 105 * (0.5 - 0.5 * math.cos(2 * math.pi * f / 45))
        frames.append(compose(assets, f, f, fo, glow, glyphs=glyphs).convert("RGB")
                      .resize((360, 360), Image.LANCZOS))
    frames[0].save(os.path.join(PREVIEW, "settings_menu.gif"), save_all=True,
                   append_images=frames[1:], duration=50, loop=0)


def main():
    install = "--install" in sys.argv
    assets = build_all()
    glyphs, space = build_glyphs()

    for d in (OUT, OUT_ROT, OUT_GLYPH):
        if os.path.isdir(d):
            shutil.rmtree(d)
        os.makedirs(d)

    worst, total, bad_edge = 0, 0, []
    for name, (im, _) in sorted(assets.items()):
        im.save(os.path.join(OUT, name + ".png"))
        im.rotate(90, expand=True).save(os.path.join(OUT_ROT, name + ".png"))
        worst = max(worst, ncolors(im))
        total += im.width * im.height + 1024
        if edge_alpha(im) > 0:
            bad_edge.append(name)

    for code, (im, _) in glyphs.items():
        im.rotate(90, expand=True).save(os.path.join(OUT_GLYPH, f"g_{code:02X}.png"))
        worst = max(worst, ncolors(im))
        total += im.width * im.height + 1024
        if edge_alpha(im) > 0:
            bad_edge.append(f"g_{code:02X}")

    print(f"  {len(assets)} sprites + {len(glyphs)} glifos  pior paleta: {worst} RGBA "
          f"{'L8 ok' if worst <= 256 else 'ESTOUROU'}")
    print(f"  flash: {total / 1024:.0f} KB em L8_ARGB8888")
    print(f"  borda com alpha > 0: {bad_edge if bad_edge else 'nenhuma'}")
    for i in range(len(ROWS)):
        print(f"    row_{i}: {row_rect(i)}")

    previews(assets, (glyphs, space))
    print(f"  preview: {PREVIEW}/settings_menu.png / .gif / _leak.png")

    if install:
        if os.path.isdir(TGFX_IMG):
            shutil.rmtree(TGFX_IMG)
        shutil.copytree(OUT_ROT, TGFX_IMG)
        if os.path.isdir(TGFX_GLYPH):
            shutil.rmtree(TGFX_GLYPH)
        shutil.copytree(OUT_GLYPH, TGFX_GLYPH)
        write_layout(assets)
        write_glyphs(glyphs, space)
        print(f"  instalado em {TGFX_IMG}")
        print(f"  layout em {LAYOUT_HPP}")


if __name__ == "__main__":
    main()
