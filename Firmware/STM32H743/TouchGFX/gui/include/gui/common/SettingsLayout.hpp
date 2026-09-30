/* GERADO por aura_assets/gen/gen_settings.py - nao edite a mao. */
#ifndef SETTINGSLAYOUT_HPP
#define SETTINGSLAYOUT_HPP

#include <stdint.h>

/* Retangulos em coordenadas de FRAMEBUFFER (ja girados). A tela
   logica gira 90 graus: x logico -> 479 - y, y logico -> x.   */
struct SettingsRect { int16_t x, y, w, h; };

#define SL_ROWS 4

/* linhas (caixa + icone + rotulo + chevron) e halo do foco */
static const SettingsRect SL_ROW[SL_ROWS] = {
    { 132, 58, 54, 364 },
    { 186, 58, 54, 364 },
    { 240, 58, 54, 364 },
    { 294, 58, 54, 364 },
};
static const SettingsRect SL_GLOW[SL_ROWS] = {
    { 118, 44, 82, 392 },
    { 172, 44, 82, 392 },
    { 226, 44, 82, 392 },
    { 280, 44, 82, 392 },
};

/* centro logico (y) de cada linha - usado pelo foco da mao */
static const int16_t SL_ROW_CY_LOGICAL[SL_ROWS] = { 159, 213, 267, 321 };
static const int16_t SL_ROW_PITCH = 54;

/* valores (GlyphText): a borda direita LOGICA da caixa do texto
   fica nesta coordenada y do framebuffer (inclui a margem da
   celula do glifo); o texto nao passa da largura logica maxima */
static const int16_t SL_VALUE_END_FB_Y = 125;
static const int16_t SL_VALUE_MAX_W[SL_ROWS] = { 166, 118, 112, 155 };

static const int16_t SL_SUB_W = 13, SL_SUB_H = 150;
static const int16_t SL_TITLE_CX_FB = 88;   /* centro x (fb) do titulo */
static const int16_t SL_SUB_CX_FB = 113;

#endif
