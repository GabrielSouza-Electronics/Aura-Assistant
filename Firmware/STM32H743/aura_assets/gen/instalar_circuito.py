#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Instala os assets ROTACIONADOS do tema circuito no projeto TouchGFX.

    python instalar_circuito.py ../TouchGFX

Copia de rot/ para assets/images/aura/ e escreve o application.config com
todos em L8_ARGB8888, sem compressao, em ExtFlashSection.

ESTRUTURA DE PASTAS — importa para o Designer
Cada sequencia numerada fica na sua propria pasta, porque o codigo faz
aritmetica com os IDs (BITMAP_X_00_ID + i). Um arquivo avulso numa dessas
pastas quebra a sequencia sem dar erro de compilacao.
"""

import os, sys, json, shutil

HERE = os.path.dirname(os.path.abspath(__file__))

# O script mora em gen/, mas os assets ficam um nivel acima, em
# assets_rotacionados/. Aceito tambem 'rot/' para quem gerou com o
# gen_rotate.py sem renomear.
_BASE = os.path.dirname(HERE)
SRC = None
for _cand in ("assets_rotacionados", "rot"):
    _p = os.path.join(_BASE, _cand)
    if os.path.isdir(_p):
        SRC = _p
        break
if SRC is None:
    SRC = os.path.join(_BASE, "assets_rotacionados")

# (pasta de origem em rot/, destino em assets/images/)
GROUPS = [
    ("circuit",        "aura/circuit"),
    ("icons",          "aura/icons"),
    ("ring",           "aura/ring"),
    ("text",           "aura/text"),
    ("logo",           "aura/logo"),
    ("hero",           "aura/hero"),
    ("status",         "aura/status"),
    ("divider",        "aura/divider"),
    ("rim",            "aura/rim"),
    ("settings",       "aura/settings"),   # gerado ja girado: gen_settings.py
]


def collect():
    out = {}
    for src, dst in GROUPS:
        d = os.path.join(SRC, src)
        if not os.path.isdir(d):
            continue
        for root, _, files in os.walk(d):
            for f in sorted(files):
                if not f.endswith(".png"):
                    continue
                rel = os.path.relpath(os.path.join(root, f), d)
                out[f"{dst}/{rel}".replace("\\", "/")] = os.path.join(root, f)
    return out


def main():
    if not os.path.isdir(SRC):
        print("NAO ENCONTREI OS ASSETS ROTACIONADOS\n")
        print(f"  procurei em: {SRC}")
        print("\n  Rode antes:  python gen/gen_rotate.py")
        sys.exit(1)

    files = collect()
    if not files:
        print("NENHUM PNG ENCONTRADO\n")
        print(f"  em: {SRC}")
        print("\n  Rode antes:  python gen/gen_rotate.py")
        sys.exit(1)
    if len(sys.argv) < 2:
        print(f"{len(files)} imagens. Passe o caminho do projeto TouchGFX.")
        return

    proj = os.path.abspath(sys.argv[1])
    base = os.path.join(proj, "assets", "images")

    # Valida ANTES de mexer em qualquer coisa. Antes isto era so um aviso e o
    # script seguia, instalando 0 imagens e quebrando ao escrever o config -
    # um erro confuso no fim em vez de uma mensagem clara no inicio.
    problemas = []
    if not os.path.isdir(proj):
        problemas.append(f"a pasta nao existe: {proj}")
    else:
        for exigido in ("assets", "gui"):
            if not os.path.isdir(os.path.join(proj, exigido)):
                problemas.append(f"nao existe '{exigido}/' dentro dela")

    if problemas:
        print("CAMINHO DO PROJETO INVALIDO\n")
        for p in problemas:
            print(f"  - {p}")
        print(f"\n  voce passou : {sys.argv[1]}")
        print(f"  que resolve : {proj}")
        print("\n  A raiz do projeto TouchGFX e a pasta que contem 'assets/',")
        print("  'gui/' e o application.config.")
        print("\n  Se este script esta em TouchGFX/aura_assets/, o destino e:")
        print("      python gen/instalar_circuito.py ..")
        print("\n  Se esta em Firmware/STM32H743/aura_assets/, e:")
        print("      python gen/instalar_circuito.py ../TouchGFX")
        sys.exit(1)

    # limpa a pasta aura antes: sobras de temas antigos geram IDs a mais e
    # quebram a aritmetica das sequencias
    aura = os.path.join(base, "aura")
    if os.path.isdir(aura):
        shutil.rmtree(aura)
        print("  pasta aura/ anterior removida")

    for rel, src in files.items():
        dst = os.path.join(base, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copyfile(src, dst)
    print(f"  {len(files)} imagens instaladas em {base}/aura/")

    cfg_path = os.path.join(proj, "application.config")
    cfg = {}
    if os.path.exists(cfg_path):
        try:
            cfg = json.load(open(cfg_path))
        except Exception:
            print("  AVISO: application.config existente nao e JSON valido; "
                  "sera reescrito")
            cfg = {}
    else:
        print("  application.config nao existia; criando")

    # LAYOUT ROTATION 90 em todas.
    #
    # A aplicacao esta em portrait e o painel e' nativamente landscape. Nesse
    # caso a documentacao do TouchGFX manda aplicar rotate90 na geracao dos
    # assets - e o que esta marca faz, por imagem, em vez do flag global.
    #
    # As imagens em rot/ ja saem giradas 90 a esquerda pelo gen_rotate.py.
    # As duas coisas trabalham juntas: o PNG girado casa com a varredura do
    # painel, e a marca diz ao image converter como interpretar os dados.
    images = {}
    for rel in files:
        images[rel] = {
            "format": "L8_ARGB8888",
            "l8_compression": "none",
            "rotate90": True,
        }

    cfg.setdefault("image_configuration", {})
    cfg["image_configuration"]["images"] = images
    cfg["image_configuration"]["dither_algorithm"] = "off"
    cfg["image_configuration"]["opaque_image_format"] = "RGB565"
    cfg["image_configuration"]["non_opaque_image_format"] = "ARGB8888"
    cfg["image_configuration"]["section"] = "ExtFlashSection"
    cfg["image_configuration"]["extra_section"] = "ExtFlashSection"

    json.dump(cfg, open(cfg_path, "w"), indent=2)
    print(f"  application.config escrito: {len(images)} imagens em L8_ARGB8888")
    print("\n  Agora abra o Designer e clique Generate Code.")
    print()
    print("  CONFIRA na aba Images que a coluna 'Layout Rotation' mostra 90")
    print("  em todas. Se estiver vazia, a chave do application.config tem")
    print("  outro nome nesta versao do TouchGFX: abra o arquivo, veja como")
    print("  suas imagens ja configuradas aparecem, e troque 'rotate90' no")
    print("  topo deste script pela chave correta.")


if __name__ == "__main__":
    main()
