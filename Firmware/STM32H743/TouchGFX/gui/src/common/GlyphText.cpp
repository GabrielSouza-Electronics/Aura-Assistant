#include <gui/common/GlyphText.hpp>
#include <gui/common/SettingsGlyphs.hpp>
#include <touchgfx/hal/HAL.hpp>
#include <touchgfx/Bitmap.hpp>

using namespace touchgfx;

static const int GLYPH_COUNT = (int)(sizeof(SG_GLYPHS) / sizeof(SG_GLYPHS[0]));
static const uint8_t SPACE = 0xFF;

static uint8_t findGlyph(char c)
{
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    if (c == ' ') return SPACE;
    for (int i = 0; i < GLYPH_COUNT; i++)
    {
        if (SG_GLYPHS[i].c == c) return (uint8_t)i;
    }
    for (int i = 0; i < GLYPH_COUNT; i++)
    {
        if (SG_GLYPHS[i].c == '?') return (uint8_t)i;
    }
    return SPACE;
}

GlyphText::GlyphText()
    : count(0), alpha(255), extent(0)
{
}

/* Monta os n primeiros caracteres (mais ".." se dots). Devolve 1 se
   coube em maxW. Avancos em 1/16 px, acumulados sem arredondar.      */
int GlyphText::layout(const char* text, int n, bool dots, int16_t maxW)
{
    int32_t x16 = 0;
    count = 0;
    extent = 0;
    for (int i = 0; i < n + (dots ? 2 : 0) && count < GT_MAX_CHARS; i++)
    {
        const uint8_t g = findGlyph(i < n ? text[i] : '.');
        if (g == SPACE)
        {
            x16 += SG_SPACE_ADV16;
            continue;
        }
        const int16_t p = (int16_t)((x16 + 8) >> 4);
        glyph[count] = g;
        pos[count] = p;
        count++;
        const int16_t end = (int16_t)(p + SG_GLYPHS[g].cellW);
        if (end > extent) extent = end;
        x16 += SG_GLYPHS[g].adv16;
    }
    return (extent <= maxW + 2 * SG_CELL_PAD) ? 1 : 0;
}

void GlyphText::setText(const char* text, int16_t maxW)
{
    int n = 0;
    while (text && text[n] != '\0' && n < GT_MAX_CHARS) n++;

    if (!layout(text, n, false, maxW))
    {
        /* corta ate caber junto com ".." */
        while (n > 0)
        {
            n--;
            while (n > 0 && text[n - 1] == ' ') n--;
            if (layout(text, n, true, maxW)) break;
        }
    }
    setWidthHeight(SG_TEXT_H, extent);
}

void GlyphText::draw(const Rect& invalidatedArea) const
{
    if (alpha == 0) return;
    for (int i = 0; i < count; i++)
    {
        const SettingsGlyph& g = SG_GLYPHS[glyph[i]];
        /* x logico cresce -> y do framebuffer decresce */
        Rect r(0, (int16_t)(extent - (pos[i] + g.cellW)), SG_TEXT_H, g.cellW);
        Rect dirty = r & invalidatedArea;
        if (dirty.isEmpty()) continue;
        dirty.x = (int16_t)(dirty.x - r.x);
        dirty.y = (int16_t)(dirty.y - r.y);
        translateRectToAbsolute(r);
        HAL::lcd().drawPartialBitmap(Bitmap(g.id), r.x, r.y, dirty, alpha, true);
    }
}
