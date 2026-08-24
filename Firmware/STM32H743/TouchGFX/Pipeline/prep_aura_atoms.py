#!/usr/bin/env python3
"""
prep_aura_atoms.py - Decompoe o menu AURA em sprites atomicos animaveis.

Cada elemento vira uma imagem separada: estrelas, nebulosas, orbitas, nucleo,
disco de vidro dos nos, icones, pilulas, header e footer. Nada de tela cheia
por estado - o C++ compoe e anima cada peca com opacidade e posicao proprias.

Duas categorias de sprite:

  ABSOLUTOS   - posicao fixa (orbitas, nucleo, header, footer, pilulas).
                Cada um sabe seu x,y no canvas 480x480.

  REUTILIZAVEIS - um bitmap usado por N instancias (estrelas, glow dots,
                marcadores de orbita, disco de vidro dos nos). O bitmap vai
                uma vez para a flash; as instancias sao so uma tabela de
                coordenadas e alpha.

E dai que vem o ganho: 210 estrelas custam 6 bitmaps minusculos, nao 210.

COMO USAR:
    pip install pillow numpy cairosvg
    python prep_aura_atoms.py

Saida em atoms/: PNGs + manifest.json + AuraAtoms.hpp
"""

from __future__ import annotations

import copy
import io
import json
import math
import xml.etree.ElementTree as ET
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

import cairosvg

# ============================== CONFIGURACAO ==============================
SVG_ENTRADA = "aura_orbit.svg"
PASTA_SAIDA = "atoms"

CANVAS = 480
SUPERSAMPLE = 2        # renderiza 2x e reduz: bordas e texto bem mais limpos
DITHER = "floyd"       # so afeta o sprite opaco de fundo
CIRCULAR = True        # mascara circular no fundo (display redondo)
FUNDO = (0, 0, 0)

# Estrelas com opacidade acima disso viram sprites soltos (animaveis).
# As mais fracas sao assadas no fundo - ninguem vai ver piscar mesmo.
# 0.0 = todas soltas (210 widgets, pesado). 1.1 = todas assadas.
ESTRELA_ALPHA_MIN = 0.55

# Nebulosas sao borroes suaves. Em L8 ja ficam baratas; reduzir (ex 0.25)
# corta mais 16x, mas exige ScalableImage no TouchGFX para reamplificar.
NEBULA_ESCALA = 1.0

# Orbitas separadas dao controle individual (girar cada uma), mas cada
# elipse fina tem bbox grande e quase toda transparente. False junta as 6
# num sprite so.
ORBITAS_SEPARADAS = True
# ==========================================================================

NS = "http://www.w3.org/2000/svg"
SVGNS = "{%s}" % NS
ET.register_namespace("", NS)

# ---- mapa dos indices dentro de <g clip-path="url(#disc)"> ----------------
IDX_DISCO_FUNDO = 0
IDX_NEBULAS = 1                 # <g> com 4 <ellipse>
IDX_ESTRELAS = (2, 211)
IDX_GLOWDOTS = (212, 223)       # 6 pares: halo borrado + nucleo branco
IDX_ORBITAS = (224, 229)        # 6 elipses
IDX_MARCADORES = (230, 237)     # 4 pares
IDX_NUCLEO = {
    "core_glow":   (238, 238),
    "core_discos": (239, 241),  # elipses de "chao"
    "core_aneis":  (242, 243),
    "core_bojo":   (244, 247),
    "core_arcos":  (248, 251),
    "core_texto":  (252, 252),
    "core_haste":  (253, 253),
}
IDX_HEADER = {
    "hdr_anel":    (324, 326),
    "hdr_titulo":  (327, 327),
    "hdr_wifi":    (328, 331),
    "hdr_bateria": (332, 334),
}
IDX_SETA_LATERAL = (335, 336)   # setas up/down nas bordas esq. e dir.
IDX_FOOTER = {
    "ftr_icone_lr": (337, 337),
    "ftr_texto_lr": (338, 339),
    "ftr_icone_ud": (340, 340),
    "ftr_texto_ud": (341, 342),
    "ftr_icone_hd": (343, 345),
    "ftr_texto_hd": (346, 347),
}
IDX_DIVISORES = (348, 349)
IDX_DOT_ATIVO = (350, 351)
IDX_DOT_INATIVO = (352, 355)

# Blocos-template dos nos
TPL_VIDRO_IDLE = (254, 261)     # halo + vidro + rim + especulares (Tasks)
TPL_VIDRO_IDLE_C = (112, 196)
TPL_PILULA_IDLE = (263, 266)
TPL_VIDRO_SEL = [(306, 315), (317, 320)]   # inclui os 4 arcos
TPL_VIDRO_SEL_C = (240, 112)
TPL_PILULA_SEL = (321, 323)

NOS = [
    {"id": "tasks",     "label": "Tasks",     "cx": 112, "cy": 196, "icone": 262},
    {"id": "email",     "label": "Email",     "cx": 368, "cy": 196, "icone": 275},
    {"id": "reminders", "label": "Reminders", "cx": 148, "cy": 312, "icone": 288},
    {"id": "assistant", "label": "Assistant", "cx": 332, "cy": 312, "icone": 301},
    {"id": "calendar",  "label": "Calendar",  "cx": 240, "cy": 112, "icone": 316},
]
PILL_IDLE = (20.0, 5.7)
PILL_SEL = (20.0, 6.6)
ESC_ICONE_IDLE = 1.176
ESC_ICONE_SEL = 1.490


# ------------------------------------------------------------ RGB565

def to_565(rgb: np.ndarray) -> np.ndarray:
    out = np.empty_like(rgb)
    r5, g6, b5 = rgb[..., 0] >> 3, rgb[..., 1] >> 2, rgb[..., 2] >> 3
    out[..., 0] = (r5 << 3) | (r5 >> 2)
    out[..., 1] = (g6 << 2) | (g6 >> 4)
    out[..., 2] = (b5 << 3) | (b5 >> 2)
    return out


KERNELS = {
    "floyd":    ([(1, 0, 7), (-1, 1, 3), (0, 1, 5), (1, 1, 1)], 16),
    "atkinson": ([(1, 0, 1), (2, 0, 1), (-1, 1, 1), (0, 1, 1),
                  (1, 1, 1), (0, 2, 1)], 8),
    "stucki":   ([(1, 0, 8), (2, 0, 4), (-2, 1, 2), (-1, 1, 4), (0, 1, 8),
                  (1, 1, 4), (2, 1, 2), (-2, 2, 1), (-1, 2, 2), (0, 2, 4),
                  (1, 2, 2), (2, 2, 1)], 42),
}


def quantiza(rgb: np.ndarray, kernel: str) -> np.ndarray:
    if kernel == "none":
        return to_565(rgb)
    taps, div = KERNELS[kernel]
    buf = rgb.astype(np.float32)
    h, w = buf.shape[:2]
    out = np.empty((h, w, 3), dtype=np.uint8)
    for y in range(h):
        for x in range(w):
            old = buf[y, x]
            new = to_565(np.clip(old, 0, 255).astype(np.uint8)
                         .reshape(1, 1, 3)).reshape(3).astype(np.float32)
            out[y, x] = new.astype(np.uint8)
            err = (old - new) / div
            for dx, dy, wgt in taps:
                nx, ny = x + dx, y + dy
                if 0 <= nx < w and 0 <= ny < h:
                    buf[ny, nx] += err * wgt
    return out


# ------------------------------------------------------------ SVG

class Fatiador:
    def __init__(self, caminho: Path):
        self.raiz = ET.parse(caminho).getroot()
        self.defs = self.raiz.find(SVGNS + "defs")
        self.rect_fundo = self.raiz.find(SVGNS + "rect")
        self.grupo = next(f for f in self.raiz
                          if f.tag == SVGNS + "g" and "clip-path" in f.attrib)
        self.itens = list(self.grupo)

    def faixa(self, ini, fim):
        return [copy.deepcopy(e) for e in self.itens[ini:fim + 1]]

    def item(self, i):
        return copy.deepcopy(self.itens[i])

    def monta(self, elementos, clip=False, fundo=False):
        svg = ET.Element(SVGNS + "svg", {
            "width": str(CANVAS), "height": str(CANVAS),
            "viewBox": f"0 0 {CANVAS} {CANVAS}"})
        if self.defs is not None:
            svg.append(copy.deepcopy(self.defs))
        if fundo and self.rect_fundo is not None:
            svg.append(copy.deepcopy(self.rect_fundo))
        attrs = dict(self.grupo.attrib) if clip else {}
        g = ET.SubElement(svg, SVGNS + "g", attrs)
        for e in elementos:
            g.append(e)
        return ET.tostring(svg, encoding="utf-8", xml_declaration=True)


def renderiza(svg_bytes, escala=SUPERSAMPLE, saida_px=CANVAS):
    px = int(round(saida_px * escala))
    png = cairosvg.svg2png(bytestring=svg_bytes, output_width=px,
                           output_height=px)
    img = Image.open(io.BytesIO(png)).convert("RGBA")
    if img.size != (saida_px, saida_px):
        img = img.resize((saida_px, saida_px), Image.LANCZOS)
    return img


def recorta(img):
    bb = img.getbbox()
    if bb is None:
        return img, 0, 0
    return img.crop(bb), bb[0], bb[1]


def mascara_circular(size):
    ss = 4
    big = Image.new("L", (size * ss, size * ss), 0)
    ImageDraw.Draw(big).ellipse((0, 0, size * ss - 1, size * ss - 1), fill=255)
    return big.resize((size, size), Image.LANCZOS)


def sugere_formato(img: Image.Image):
    """Escolhe o formato mais barato que preserva a imagem.

    RGB565        - totalmente opaca
    L8_ARGB8888   - ate 256 cores distintas (tipico de traco de cor unica
                    com antialiasing: so o alpha varia). 1 byte/px + paleta.
    ARGB8888      - o resto (gradientes com transparencia)
    """
    a = np.array(img)
    if a.shape[2] == 4 and (a[..., 3] == 255).all():
        return "RGB565"
    cores = np.unique(a.reshape(-1, a.shape[2]), axis=0)
    area = img.width * img.height
    if len(cores) <= 256 and area * 3 > 1024:
        return "L8_ARGB8888"
    return "ARGB8888"


def custo(formato, w, h):
    if formato == "RGB565":
        return w * h * 2
    if formato == "L8_ARGB8888":
        return w * h + 256 * 4
    return w * h * 4


def humano(n):
    return f"{n/1024:.1f} KB" if n < 1048576 else f"{n/1048576:.2f} MB"


# ------------------------------------------------------------ pipeline

class Pipeline:
    def __init__(self, fat: Fatiador, saida: Path):
        self.fat = fat
        self.saida = saida
        self.sprites = []       # bitmaps que vao para a flash
        self.absolutos = []     # instancias de posicao fixa
        self.instancias = {}    # nome_sprite -> lista de instancias
        self.total = 0

    # ---- registro -----------------------------------------------------
    def _grava(self, nome, img, formato=None):
        img.save(self.saida / f"{nome}.png")
        w, h = img.size
        if formato is None:
            formato = sugere_formato(img)
        nb = custo(formato, w, h)
        self.total += nb
        self.sprites.append({"nome": nome, "arquivo": f"{nome}.png",
                             "formato": formato, "w": w, "h": h, "bytes": nb})
        return w, h

    def atomo(self, nome, elementos, clip=False, escala_render=1.0):
        """Sprite de posicao absoluta."""
        px = CANVAS
        img = renderiza(self.fat.monta(elementos, clip=clip))
        img, x, y = recorta(img)
        if escala_render != 1.0:
            nw = max(1, int(round(img.width * escala_render)))
            nh = max(1, int(round(img.height * escala_render)))
            img = img.resize((nw, nh), Image.LANCZOS)
        w, h = self._grava(nome, img)
        self.absolutos.append({"sprite": nome, "x": x, "y": y,
                               "w": w, "h": h, "escala": escala_render})
        return nome

    def sprite_livre(self, nome, elementos, cx=None, cy=None):
        """Bitmap reutilizavel. Devolve (nome, ox, oy) relativos ao centro."""
        img = renderiza(self.fat.monta(elementos))
        img, x, y = recorta(img)
        self._grava(nome, img)
        ox = x - cx if cx is not None else x
        oy = y - cy if cy is not None else y
        return nome, ox, oy

    def instancia(self, sprite, x, y, alpha=255, tag=""):
        self.instancias.setdefault(sprite, []).append(
            {"x": int(x), "y": int(y), "alpha": int(alpha), "tag": tag})


# ------------------------------------------------------------ etapas

def estrelas_tabela(fat):
    """Le as 210 estrelas: devolve lista (cx, cy, r, opacidade)."""
    out = []
    a, b = IDX_ESTRELAS
    for i in range(a, b + 1):
        e = fat.itens[i]
        out.append((float(e.attrib["cx"]), float(e.attrib["cy"]),
                    float(e.attrib["r"]), float(e.attrib.get("opacity", 1))))
    return out


def sprite_estrela(pipe, raio):
    """Circulo branco solido de raio r, para receber alpha no C++."""
    lado = max(4, int(math.ceil(raio * 2)) + 4)
    ss = 8
    img = Image.new("RGBA", (lado * ss, lado * ss), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c = lado * ss / 2.0
    rr = raio * ss
    d.ellipse((c - rr, c - rr, c + rr, c + rr), fill=(227, 246, 255, 255))
    img = img.resize((lado, lado), Image.LANCZOS)
    nome = f"star_{str(raio).replace('.', 'p')}"
    pipe._grava(nome, img)
    return nome, lado


def main():
    entrada = Path(SVG_ENTRADA)
    if not entrada.is_file():
        print(f"ERRO: '{SVG_ENTRADA}' nao encontrado nesta pasta.")
        return

    saida = Path(PASTA_SAIDA)
    saida.mkdir(parents=True, exist_ok=True)

    fat = Fatiador(entrada)
    pipe = Pipeline(fat, saida)

    # ---------- 1. disco de fundo (unico sprite opaco) ----------------
    estrelas = estrelas_tabela(fat)
    fracas = [i for i, s in enumerate(estrelas) if s[3] < ESTRELA_ALPHA_MIN]
    fortes = [i for i, s in enumerate(estrelas) if s[3] >= ESTRELA_ALPHA_MIN]

    base = [fat.item(IDX_DISCO_FUNDO)]
    base += [fat.item(IDX_ESTRELAS[0] + i) for i in fracas]

    img = renderiza(fat.monta(base, clip=True, fundo=True))
    fundo_img = Image.new("RGBA", img.size, FUNDO + (255,))
    plana = Image.alpha_composite(fundo_img, img).convert("RGB")
    rgb = np.array(plana, dtype=np.uint8)
    quant = quantiza(rgb, DITHER)
    bg = Image.fromarray(quant, "RGB")
    if CIRCULAR:
        m = mascara_circular(CANVAS)
        b2 = Image.new("RGB", bg.size, FUNDO)
        b2.paste(bg, (0, 0), m)
        bg = b2
    pipe._grava("bg_space", bg, "RGB565")
    pipe.absolutos.append({"sprite": "bg_space", "x": 0, "y": 0,
                           "w": CANVAS, "h": CANVAS, "escala": 1.0})

    # ---------- 2. estrelas fortes: sprites reutilizaveis -------------
    raios = sorted({estrelas[i][2] for i in fortes})
    mapa_estrela = {}
    for r in raios:
        nome, lado = sprite_estrela(pipe, r)
        mapa_estrela[r] = (nome, lado)
    for i in fortes:
        cx, cy, r, op = estrelas[i]
        nome, lado = mapa_estrela[r]
        pipe.instancia(nome, round(cx - lado / 2), round(cy - lado / 2),
                       int(round(op * 255)), "star")

    # ---------- 3. nebulosas ------------------------------------------
    grupo_neb = fat.item(IDX_NEBULAS)
    filhos = list(grupo_neb)
    for k, filho in enumerate(filhos):
        g = ET.Element(SVGNS + "g", dict(grupo_neb.attrib))
        g.append(copy.deepcopy(filho))
        pipe.atomo(f"nebula_{k}", [g], escala_render=NEBULA_ESCALA)

    # ---------- 4. glow dots ------------------------------------------
    a, b = IDX_GLOWDOTS
    vistos = {}
    for i in range(a, b + 1, 2):
        halo, nucleo = fat.item(i), fat.item(i + 1)
        chave = (halo.attrib["r"], nucleo.attrib["r"])
        cx, cy = float(halo.attrib["cx"]), float(halo.attrib["cy"])
        if chave not in vistos:
            h2, n2 = copy.deepcopy(halo), copy.deepcopy(nucleo)
            for e in (h2, n2):
                e.set("cx", "40")
                e.set("cy", "40")
            nome, ox, oy = pipe.sprite_livre(
                f"glowdot_{len(vistos)}", [h2, n2], 40, 40)
            vistos[chave] = (nome, ox, oy)
        nome, ox, oy = vistos[chave]
        pipe.instancia(nome, round(cx + ox), round(cy + oy), 255, "glowdot")

    # ---------- 5. orbitas --------------------------------------------
    a, b = IDX_ORBITAS
    if ORBITAS_SEPARADAS:
        for k, i in enumerate(range(a, b + 1)):
            pipe.atomo(f"orbit_{k}", [fat.item(i)])
    else:
        pipe.atomo("orbit_all", fat.faixa(a, b))

    # ---------- 6. marcadores de orbita -------------------------------
    a, b = IDX_MARCADORES
    marc = None
    for i in range(a, b + 1, 2):
        halo, anel = fat.item(i), fat.item(i + 1)
        cx, cy = float(halo.attrib["cx"]), float(halo.attrib["cy"])
        if marc is None:
            h2, a2 = copy.deepcopy(halo), copy.deepcopy(anel)
            for e in (h2, a2):
                e.set("cx", "40")
                e.set("cy", "40")
            marc = pipe.sprite_livre("orbit_marker", [h2, a2], 40, 40)
        nome, ox, oy = marc
        pipe.instancia(nome, round(cx + ox), round(cy + oy), 255, "marker")

    # ---------- 7. nucleo central -------------------------------------
    for nome, (a, b) in IDX_NUCLEO.items():
        pipe.atomo(nome, fat.faixa(a, b))

    # ---------- 8. disco de vidro dos nos (reutilizavel) --------------
    vidro_idle = pipe.sprite_livre("node_glass_idle",
                                   fat.faixa(*TPL_VIDRO_IDLE),
                                   *TPL_VIDRO_IDLE_C)
    sel_els = []
    for a, b in TPL_VIDRO_SEL:
        sel_els += fat.faixa(a, b)
    vidro_sel = pipe.sprite_livre("node_glass_sel", sel_els, *TPL_VIDRO_SEL_C)

    for no in NOS:
        for nome, ox, oy in (vidro_idle, vidro_sel):
            pipe.instancia(nome, round(no["cx"] + ox), round(no["cy"] + oy),
                           255, f"glass:{no['id']}")

    # ---------- 9. icones (dois tamanhos) -----------------------------
    for no in NOS:
        for suf, esc in (("idle", ESC_ICONE_IDLE), ("sel", ESC_ICONE_SEL)):
            ic = fat.item(no["icone"])
            ic.set("transform", f"translate(240,240) scale({esc})")
            nome, ox, oy = pipe.sprite_livre(
                f"icon_{no['id']}_{suf}", [ic], 240, 240)
            pipe.instancia(nome, round(no["cx"] + ox), round(no["cy"] + oy),
                           255, f"icon:{no['id']}")

    # ---------- 10. pilulas -------------------------------------------
    for no in NOS:
        for suf, tpl, centro, (pi, ps) in (
                ("idle", TPL_PILULA_IDLE, TPL_VIDRO_IDLE_C, PILL_IDLE),
                ("sel",  TPL_PILULA_SEL,  TPL_VIDRO_SEL_C,  PILL_SEL)):
            bloco = fat.faixa(*tpl)
            bx = centro[0]
            larg = pi + ps * len(no["label"])
            for e in bloco:
                tag = e.tag.replace(SVGNS, "")
                if tag == "rect":
                    e.set("x", f"{bx - larg/2:.2f}")
                    e.set("width", f"{larg:.2f}")
                elif tag == "text":
                    e.text = no["label"]
                    e.set("x", str(bx))
            dx, dy = no["cx"] - centro[0], no["cy"] - centro[1]
            if dx or dy:
                wrap = ET.Element(SVGNS + "g",
                                  {"transform": f"translate({dx},{dy})"})
                for e in bloco:
                    wrap.append(e)
                bloco = [wrap]
            pipe.atomo(f"pill_{no['id']}_{suf}", bloco)

    # ---------- 11. header --------------------------------------------
    for nome, (a, b) in IDX_HEADER.items():
        pipe.atomo(nome, fat.faixa(a, b))

    # ---------- 12. setas laterais (mesmo desenho nos dois lados) ------
    a, b = IDX_SETA_LATERAL
    seta = None
    for i in range(a, b + 1):
        el = fat.item(i)
        img = renderiza(fat.monta([el]))
        _, x, y = recorta(img)
        if seta is None:
            seta = pipe.sprite_livre("side_arrow", [el])
            base_x, base_y = x, y
        pipe.instancia(seta[0], x, y, 255, "side_arrow")

    # ---------- 13. rodape --------------------------------------------
    for nome, (a, b) in IDX_FOOTER.items():
        pipe.atomo(nome, fat.faixa(a, b))

    a, b = IDX_DIVISORES
    div = None
    for i in range(a, b + 1):
        el = fat.item(i)
        img = renderiza(fat.monta([el]))
        _, x, y = recorta(img)
        if div is None:
            div = pipe.sprite_livre("ftr_divisor", [el])
        pipe.instancia(div[0], x, y, 255, "divisor")

    # page dots: um sprite ativo, um inativo, instanciados por posicao
    a, b = IDX_DOT_ATIVO
    els = fat.faixa(a, b)
    cx = float(fat.itens[a].attrib["cx"])
    cy = float(fat.itens[a].attrib["cy"])
    for e in els:
        e.set("cx", "40")
        e.set("cy", "40")
    dot_on = pipe.sprite_livre("dot_ativo", els, 40, 40)
    pipe.instancia(dot_on[0], round(cx + dot_on[1]), round(cy + dot_on[2]),
                   255, "dot")

    a, b = IDX_DOT_INATIVO
    dot_off = None
    for i in range(a, b + 1):
        el = fat.item(i)
        cx = float(el.attrib["cx"])
        cy = float(el.attrib["cy"])
        if dot_off is None:
            e2 = copy.deepcopy(el)
            e2.set("cx", "40")
            e2.set("cy", "40")
            dot_off = pipe.sprite_livre("dot_inativo", [e2], 40, 40)
        pipe.instancia(dot_off[0], round(cx + dot_off[1]),
                       round(cy + dot_off[2]), 255, "dot")

    # ---------- relatorio e artefatos ---------------------------------
    escreve(pipe, saida, len(fortes), len(fracas))


def escreve(pipe, saida, n_soltas, n_assadas):
    manifesto = {
        "canvas": CANVAS,
        "sprites": pipe.sprites,
        "absolutos": pipe.absolutos,
        "instancias": pipe.instancias,
        "nos": NOS,
    }
    (saida / "manifest.json").write_text(
        json.dumps(manifesto, indent=2, ensure_ascii=False), encoding="utf-8")

    # ---- header C++ ---------------------------------------------------
    L = ["// Gerado por prep_aura_atoms.py - nao editar a mao.",
         "", "#ifndef AURA_ATOMS_HPP", "#define AURA_ATOMS_HPP",
         "", "#include <cstdint>", "", "namespace aura", "{",
         f"static const int16_t CANVAS_W = {CANVAS};",
         f"static const int16_t CANVAS_H = {CANVAS};", "",
         "struct Placed   { int16_t x, y, w, h; };",
         "struct Instance { int16_t x, y; uint8_t alpha; };", ""]

    for a in pipe.absolutos:
        n = a["sprite"].upper()
        L.append(f"static const Placed POS_{n} = "
                 f"{{ {a['x']:>3}, {a['y']:>3}, {a['w']:>3}, {a['h']:>3} }};")
    L.append("")

    for sprite, insts in pipe.instancias.items():
        tags = {i["tag"].split(":")[0] for i in insts}
        if "glass" in tags or "icon" in tags:
            continue  # os nos usam as tabelas GLASS_* / ICON_* abaixo
        n = sprite.upper()
        L.append(f"static const int N_{n} = {len(insts)};")
        L.append(f"static const Instance INST_{n}[N_{n}] = {{")
        for i in insts:
            L.append(f"    {{ {i['x']:>3}, {i['y']:>3}, {i['alpha']:>3} }},")
        L.append("};")
        L.append("")

    L.append(f"static const int NUM_NODES = {len(NOS)};")
    L.append("enum NodeId {")
    for k, no in enumerate(NOS):
        L.append(f"    NODE_{no['id'].upper()} = {k},")
    L.append("};")
    L.append("")
    L.append("// centro de cada no no canvas")
    L.append("static const Instance NODE_CENTER[NUM_NODES] = {")
    for no in NOS:
        L.append(f"    {{ {no['cx']:>3}, {no['cy']:>3}, 255 }},"
                 f"  // {no['label']}")
    L.append("};")
    L.append("")

    def tabela(prefixo, comentario, filtro):
        for estado in ("idle", "sel"):
            L.append(f"// {comentario} ({estado})")
            L.append(f"static const Instance {prefixo}_{estado.upper()}"
                     f"[NUM_NODES] = {{")
            for no in NOS:
                sp, inst = filtro(no, estado)
                L.append(f"    {{ {inst['x']:>3}, {inst['y']:>3}, 255 }},"
                         f"  // {no['label']}")
            L.append("};")
            L.append("")

    def acha_glass(no, estado):
        sp = f"node_glass_{estado}"
        alvo = f"glass:{no['id']}"
        return sp, next(i for i in pipe.instancias[sp] if i["tag"] == alvo)

    def acha_icon(no, estado):
        sp = f"icon_{no['id']}_{estado}"
        return sp, pipe.instancias[sp][0]

    tabela("GLASS", "canto sup-esq do disco de vidro", acha_glass)
    tabela("ICON", "canto sup-esq do icone", acha_icon)

    L += ["}  // namespace aura", "", "#endif  // AURA_ATOMS_HPP", ""]
    (saida / "AuraAtoms.hpp").write_text("\n".join(L), encoding="utf-8")

    # ---- relatorio ----------------------------------------------------
    print(f"{'sprite':<26}{'tam':>11}  {'formato':<13}{'inst':>6}{'bytes':>11}")
    print("-" * 66)
    n_inst = {s: len(v) for s, v in pipe.instancias.items()}
    for s in pipe.sprites:
        q = n_inst.get(s["nome"], 1)
        print(f"{s['nome'][:25]:<26}{f'{s[chr(119)]}x{s[chr(104)]}':>11}  "
              f"{s['formato']:<13}{q:>6}{humano(s['bytes']):>11}")
    print("-" * 66)
    print(f"{'TOTAL':<26}{'':>11}  {'':<13}{'':>6}{humano(pipe.total):>11}")

    print(f"\n{len(pipe.sprites)} bitmaps, "
          f"{sum(n_inst.values()) + len(pipe.absolutos)} instancias.")
    print(f"Estrelas: {n_soltas} soltas (animaveis), "
          f"{n_assadas} assadas no fundo.")
    cheio = CANVAS * CANVAS * 2 * (len(NOS) + 1)
    print(f"Uma imagem cheia por estado seria {humano(cheio)}. "
          f"Economia: {100*(1-pipe.total/cheio):.0f}%")
    print(f"\nSalvo em: {saida.resolve()}")


if __name__ == "__main__":
    main()
