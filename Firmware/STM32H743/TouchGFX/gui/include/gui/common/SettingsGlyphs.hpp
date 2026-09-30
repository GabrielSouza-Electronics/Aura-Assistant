/* GERADO por aura_assets/gen/gen_settings.py - nao edite a mao. */
#ifndef SETTINGSGLYPHS_HPP
#define SETTINGSGLYPHS_HPP

#include <stdint.h>
#include <images/BitmapDatabase.hpp>

/* Atlas de glifos dos valores do menu Settings (Poppins Medium
   18 pt, tracking do gen_text.py). Celulas ja giradas: no
   framebuffer a largura e a altura do texto e a altura e a
   largura LOGICA da celula. Avancos em 1/16 px logico.       */
struct SettingsGlyph { char c; uint16_t id; uint8_t cellW; uint16_t adv16; };

static const int16_t SG_TEXT_H = 30;   /* largura no fb */
static const int16_t SG_CELL_PAD = 3;
static const uint8_t SG_SPACE_ADV16 = 81;

/* ordenado por codigo ASCII */
static const SettingsGlyph SG_GLYPHS[54] = {
    { '!', BITMAP_G_21_ID, 13, 98 },
    { '#', BITMAP_G_23_ID, 23, 257 },
    { '%', BITMAP_G_25_ID, 21, 234 },
    { '&', BITMAP_G_26_ID, 21, 225 },
    { '\'', BITMAP_G_27_ID, 10, 55 },
    { '(', BITMAP_G_28_ID, 16, 147 },
    { ')', BITMAP_G_29_ID, 16, 147 },
    { '*', BITMAP_G_2A_ID, 16, 151 },
    { '+', BITMAP_G_2B_ID, 20, 211 },
    { ',', BITMAP_G_2C_ID, 11, 71 },
    { '-', BITMAP_G_2D_ID, 17, 174 },
    { '.', BITMAP_G_2E_ID, 11, 75 },
    { '/', BITMAP_G_2F_ID, 16, 154 },
    { '0', BITMAP_G_30_ID, 18, 190 },
    { '1', BITMAP_G_31_ID, 13, 107 },
    { '2', BITMAP_G_32_ID, 17, 172 },
    { '3', BITMAP_G_33_ID, 18, 177 },
    { '4', BITMAP_G_34_ID, 19, 192 },
    { '5', BITMAP_G_35_ID, 18, 190 },
    { '6', BITMAP_G_36_ID, 18, 191 },
    { '7', BITMAP_G_37_ID, 17, 167 },
    { '8', BITMAP_G_38_ID, 18, 190 },
    { '9', BITMAP_G_39_ID, 18, 189 },
    { ':', BITMAP_G_3A_ID, 11, 76 },
    { ';', BITMAP_G_3B_ID, 12, 93 },
    { '?', BITMAP_G_3F_ID, 17, 161 },
    { '@', BITMAP_G_40_ID, 25, 301 },
    { 'A', BITMAP_G_41_ID, 19, 207 },
    { 'B', BITMAP_G_42_ID, 18, 187 },
    { 'C', BITMAP_G_43_ID, 21, 228 },
    { 'D', BITMAP_G_44_ID, 20, 210 },
    { 'E', BITMAP_G_45_ID, 16, 157 },
    { 'F', BITMAP_G_46_ID, 16, 154 },
    { 'G', BITMAP_G_47_ID, 21, 228 },
    { 'H', BITMAP_G_48_ID, 20, 209 },
    { 'I', BITMAP_G_49_ID, 12, 82 },
    { 'J', BITMAP_G_4A_ID, 17, 168 },
    { 'K', BITMAP_G_4B_ID, 18, 188 },
    { 'L', BITMAP_G_4C_ID, 15, 134 },
    { 'M', BITMAP_G_4D_ID, 23, 260 },
    { 'N', BITMAP_G_4E_ID, 20, 213 },
    { 'O', BITMAP_G_4F_ID, 21, 232 },
    { 'P', BITMAP_G_50_ID, 18, 177 },
    { 'Q', BITMAP_G_51_ID, 21, 232 },
    { 'R', BITMAP_G_52_ID, 18, 188 },
    { 'S', BITMAP_G_53_ID, 18, 180 },
    { 'T', BITMAP_G_54_ID, 17, 168 },
    { 'U', BITMAP_G_55_ID, 19, 204 },
    { 'V', BITMAP_G_56_ID, 19, 206 },
    { 'W', BITMAP_G_57_ID, 25, 293 },
    { 'X', BITMAP_G_58_ID, 19, 196 },
    { 'Y', BITMAP_G_59_ID, 18, 180 },
    { 'Z', BITMAP_G_5A_ID, 17, 166 },
    { '_', BITMAP_G_5F_ID, 21, 236 },
};

#endif
