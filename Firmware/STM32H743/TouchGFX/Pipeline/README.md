# Menu AURA - sprites atômicos animáveis (TouchGFX / STM32N6570-DK)

Decompõe o `aura_orbit.svg` em **66 bitmaps** e **176 instâncias**: cada
estrela, nebulosa, órbita, ícone e pílula é uma peça independente com
opacidade e posição próprias. Nada é desenhado como tela cheia.

```
aura_atoms/
├── aura_orbit.svg
├── prep_aura_atoms.py          o pipeline
├── atoms/                      <- gerado
│   ├── bg_space.png            único sprite opaco (480x480 RGB565)
│   ├── star_0p45.png …         6 bitmaps de estrela → 99 instâncias
│   ├── nebula_0..3.png
│   ├── orbit_0..5.png
│   ├── core_*.png              núcleo em 7 camadas
│   ├── node_glass_idle/sel.png 2 bitmaps → 10 instâncias
│   ├── icon_<id>_idle/sel.png
│   ├── pill_<id>_idle/sel.png
│   ├── hdr_*, ftr_*, dot_*, side_arrow
│   ├── manifest.json
│   └── AuraAtoms.hpp
└── touchgfx/
    ├── Screen1View.hpp
    └── Screen1View.cpp
```

---

## 1. Duas categorias de sprite

**Absolutos** — posição fixa no canvas. Órbitas, núcleo, header, rodapé,
pílulas. Cada um vira um `Placed { x, y, w, h }` no header.

**Reutilizáveis** — um bitmap, N instâncias. Estrelas, glow dots, marcadores
de órbita, disco de vidro dos nós. O bitmap vai uma vez para a flash; as
instâncias são só uma tabela de `{ x, y, alpha }`.

É daí que vem o ganho: **210 estrelas custam 6 bitmaps minúsculos**, não 210.
Os cinco nós compartilham **um** disco de vidro idle e **um** selecionado — o
que muda entre eles é só o ícone e a pílula.

| | Flash |
|---|---|
| 6 imagens cheias 480x480 RGB565 | 2.64 MB |
| **66 sprites atômicos** | **1.38 MB** |

---

## 2. Seleção automática de formato

O script analisa cada sprite e escolhe o formato mais barato que preserva a
imagem:

| Formato | Quando | Custo |
|---|---|---|
| `RGB565` | totalmente opaco | 2 B/px |
| `L8_ARGB8888` | até 256 cores distintas | 1 B/px + 1 KB de paleta |
| `ARGB8888` | gradientes com transparência | 4 B/px |

O `L8_ARGB8888` é o que salva as órbitas. Elas são traço de cor única
(`#6FE4FF`) com antialiasing — só o **alpha** varia, então 256 entradas de
paleta cobrem tudo. Cada órbita cai de 229 KB para 58 KB. Mesma coisa nas
nebulosas, no glow do núcleo e nos ícones do rodapé.

A regra de corte: só vale a paleta se `w*h*3 > 1024`, senão o overhead de
1 KB come o ganho. Sprites pequenos ficam em ARGB8888.

---

## 3. Estrelas: quais animar

210 widgets de estrela derrubariam o frame rate. O script usa um limiar:

```python
ESTRELA_ALPHA_MIN = 0.55   # acima disso vira sprite solto
```

- **99 estrelas** acima do limiar → sprites soltos, animáveis
- **111 estrelas** abaixo → assadas no `bg_space`

As fracas ninguém vê piscar mesmo. Ajuste o limiar conforme o orçamento de
CPU: `0.0` solta todas, `1.1` assa todas.

---

## 4. As animações

No `Screen1View.cpp`, `handleTickEvent()`:

**Cintilar das estrelas.** Cada estrela tem fase e velocidade derivadas da
própria posição (`x*7 + y*13`), então o campo inteiro cintila fora de sincronia
sem precisar de RNG nem de tabela extra na flash.

**Deriva do campo.** Um deslocamento senoidal lento de ±2 px, com fase que
varia com `x`, dá a sensação de profundidade sem nenhum custo de bitmap.

**Custo espalhado.** Só `STARS_PER_TICK = 12` estrelas são atualizadas por
frame. Cada uma é revisitada a cada ~8 frames — o cintilar fica orgânico e o
custo de invalidação fica baixo. Invalidar 99 widgets todo frame não fecharia
os 60 fps.

**Nebulosas e órbitas.** Respiram em opacidade com períodos primos entre si,
para nunca sincronizarem. Atualizadas a cada 4 frames (`tick & 0x03`), que é
imperceptível e corta o custo por quatro.

**Seno inteiro.** A função `isin()` usa uma tabela de 65 entradas e simetria de
quadrante. Sem float no tick — relevante mesmo com FPU, porque evita
salvar/restaurar o contexto de FPU em interrupção.

Para desligar tudo e economizar CPU: `view.setAmbientAnimation(false)`.

---

## 5. Ordem de desenho

```
bg_space  →  nebulosas  →  estrelas  →  órbitas  →  marcadores
          →  núcleo (7 camadas)
          →  nós idle (vidro, ícone, pílula)
          →  nós sel  (vidro, ícone, pílula)
          →  header, setas laterais, rodapé
```

Os `sel` entram **depois de todos** os `idle`: o selecionado é maior
(118x120 contra 82x82) e precisa ficar na frente dos vizinhos, senão o halo é
recortado.

---

## 6. Importar no TouchGFX Designer

1. Copie os PNGs de `atoms/` para `assets/images/` do projeto
2. Na aba Images, defina o **Image Format** de cada um conforme a coluna
   `formato` do relatório (ou leia do `manifest.json`)
3. **Dither desligado** no `bg_space` — o Python já ditherizou
4. **Section** → `ExtFlashSection`. São 1.38 MB: irrelevante nos 128 MB da
   flash externa, mas em `IntFlashSection` isso viraria RAM no modo LRUN
5. Copie `AuraAtoms.hpp` para `gui/include/gui/screen1_screen/`
6. Substitua `Screen1View.hpp` / `.cpp` pelos de `touchgfx/`
7. Marque a tela para receber tick events no Designer, senão
   `handleTickEvent()` nunca é chamado

Os nomes dos bitmaps (`BITMAP_STAR_0P45_ID`, `BITMAP_ORBIT_0_ID`, ...) são
derivados pelo Designer a partir do nome do arquivo.

---

## 7. Mexer no SVG

O script localiza os elementos por **índice** dentro do grupo
`<g clip-path="url(#disc)">`. As constantes `IDX_*` no topo mapeiam tudo. Se
você reordenar elementos no SVG, ajuste esses índices.

Para adicionar um nó: desenhe o ícone como
`<g transform="translate(cx,cy) scale(s)">` e acrescente uma entrada em `NOS`
com `id`, `label`, `cx`, `cy` e o índice do grupo. O `AuraAtoms.hpp` e o
`manifest.json` se regeneram.

Os estados `idle` e `sel` de cada nó são **gerados**, não extraídos: o script
pega o template de cada estilo, troca o ícone, troca o rótulo, recalcula a
largura da pílula pelo comprimento do texto e translada para a posição do nó.
Por isso o SVG só precisa de um exemplar de cada estilo.

---

## 8. Gravar na placa

```
BOOT1 (chave de baixo) à direita   -> modo de programação
Flash Scripts/…LoadAll.bat         -> FSBL + Appli + assets
BOOT1 à esquerda                   -> modo de execução
desconectar e reconectar o USB
```

Se só o C++ mudou e os assets não, use o script que grava apenas a aplicação.
