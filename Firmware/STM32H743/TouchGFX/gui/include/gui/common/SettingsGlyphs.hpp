/* GERADO por aura_assets/gen/gen_settings.py - nao edite a mao. */
#ifndef SETTINGSGLYPHS_HPP
#define SETTINGSGLYPHS_HPP

#include <stdint.h>
#include <images/BitmapDatabase.hpp>

/* Atlas de glifos dos valores do menu Settings (Poppins Regular
   11 pt, tracking do gen_text.py). Celulas ja giradas: no
   framebuffer a largura e a altura do texto e a altura e a
   largura LOGICA da celula. Avancos em 1/16 px logico.       */
struct SettingsGlyph { char c; uint16_t id; uint8_t cellW; uint8_t adv16; };

static const int16_t SG_TEXT_H = 19;   /* largura no fb */
static const int16_t SG_CELL_PAD = 3;
static const uint8_t SG_SPACE_ADV16 = 87;

/* ordenado por codigo ASCII */
static const SettingsGlyph SG_GLYPHS[54] = {
    { '!', BITMAP_G_21_ID, 12, 91 },
    { '#', BITMAP_G_23_ID, 18, 183 },
    { '%', BITMAP_G_25_ID, 17, 175 },
    { '&', BITMAP_G_26_ID, 17, 171 },
    { '\'', BITMAP_G_27_ID, 11, 67 },
    { '(', BITMAP_G_28_ID, 14, 119 },
    { ')', BITMAP_G_29_ID, 14, 119 },
    { '*', BITMAP_G_2A_ID, 14, 123 },
    { '+', BITMAP_G_2B_ID, 16, 159 },
    { ',', BITMAP_G_2C_ID, 11, 75 },
    { '-', BITMAP_G_2D_ID, 15, 139 },
    { '.', BITMAP_G_2E_ID, 11, 75 },
    { '/', BITMAP_G_2F_ID, 14, 123 },
    { '0', BITMAP_G_30_ID, 16, 151 },
    { '1', BITMAP_G_31_ID, 12, 95 },
    { '2', BITMAP_G_32_ID, 15, 139 },
    { '3', BITMAP_G_33_ID, 16, 147 },
    { '4', BITMAP_G_34_ID, 16, 151 },
    { '5', BITMAP_G_35_ID, 16, 151 },
    { '6', BITMAP_G_36_ID, 16, 151 },
    { '7', BITMAP_G_37_ID, 15, 135 },
    { '8', BITMAP_G_38_ID, 16, 155 },
    { '9', BITMAP_G_39_ID, 16, 151 },
    { ':', BITMAP_G_3A_ID, 11, 75 },
    { ';', BITMAP_G_3B_ID, 12, 83 },
    { '?', BITMAP_G_3F_ID, 15, 131 },
    { '@', BITMAP_G_40_ID, 20, 215 },
    { 'A', BITMAP_G_41_ID, 16, 159 },
    { 'B', BITMAP_G_42_ID, 15, 143 },
    { 'C', BITMAP_G_43_ID, 17, 175 },
    { 'D', BITMAP_G_44_ID, 17, 163 },
    { 'E', BITMAP_G_45_ID, 14, 127 },
    { 'F', BITMAP_G_46_ID, 14, 127 },
    { 'G', BITMAP_G_47_ID, 18, 179 },
    { 'H', BITMAP_G_48_ID, 16, 159 },
    { 'I', BITMAP_G_49_ID, 12, 83 },
    { 'J', BITMAP_G_4A_ID, 15, 131 },
    { 'K', BITMAP_G_4B_ID, 15, 143 },
    { 'L', BITMAP_G_4C_ID, 13, 111 },
    { 'M', BITMAP_G_4D_ID, 18, 187 },
    { 'N', BITMAP_G_4E_ID, 17, 163 },
    { 'O', BITMAP_G_4F_ID, 18, 179 },
    { 'P', BITMAP_G_50_ID, 15, 139 },
    { 'Q', BITMAP_G_51_ID, 18, 179 },
    { 'R', BITMAP_G_52_ID, 15, 143 },
    { 'S', BITMAP_G_53_ID, 16, 147 },
    { 'T', BITMAP_G_54_ID, 15, 135 },
    { 'U', BITMAP_G_55_ID, 16, 155 },
    { 'V', BITMAP_G_56_ID, 16, 159 },
    { 'W', BITMAP_G_57_ID, 20, 211 },
    { 'X', BITMAP_G_58_ID, 16, 147 },
    { 'Y', BITMAP_G_59_ID, 15, 143 },
    { 'Z', BITMAP_G_5A_ID, 15, 135 },
    { '_', BITMAP_G_5F_ID, 17, 167 },
};

#endif
