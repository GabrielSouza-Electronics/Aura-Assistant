#include <gui/common/GlyphText.hpp>
#include "SettingsStyleAssets.hpp"
#include <touchgfx/widgets/Image.hpp>
#include <touchgfx/widgets/PixelDataWidget.hpp>
#include <touchgfx/hal/HAL.hpp>
#include <touchgfx/Bitmap.hpp>

using namespace touchgfx;

static const uint8_t SPACE = 0xFF;

static uint8_t findGlyph(char c)
{
    if (c == ' ') return SPACE;
    const unsigned char code = static_cast<unsigned char>(c);
    return (code >= 33 && code <= 126) ? (uint8_t)(code - 33) : (uint8_t)('?' - 33);
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
            x16 += SETTINGS_STYLE_SPACE16;
            continue;
        }
        const int16_t p = (int16_t)((x16 + 8) >> 4);
        glyph[count] = g;
        pos[count] = p;
        count++;
        const int16_t end = (int16_t)(p + settingsStyleGlyphs[g].height);
        if (end > extent) extent = end;
        x16 += settingsStyleGlyphs[g].advance16;
    }
    return (extent <= maxW + 4) ? 1 : 0;
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
    setWidthHeight(settingsStyleGlyphs[0].width, extent);
}

void GlyphText::draw(const Rect& invalidatedArea) const
{
    if (alpha == 0) return;
    for (int i = 0; i < count; i++)
    {
        const SettingsStyleSprite& g = settingsStyleGlyphs[glyph[i]];
        /* x logico cresce -> y do framebuffer decresce */
        const int16_t localY = (int16_t)(extent - (pos[i] + g.height));
        Rect r(0, localY, g.width, g.height);
        Rect dirty = r & invalidatedArea;
        if (dirty.isEmpty()) continue;
        translateRectToAbsolute(r);
        Image sprite;

        sprite.setBitmap(Bitmap(g.bitmapId));
        sprite.setPosition(r.x, r.y, g.width, g.height);
        sprite.setAlpha(alpha);
        // draw() takes widget-local coordinates, even for this temporary sprite.
        dirty.y = (int16_t)(dirty.y - localY);
        sprite.draw(dirty);
    }
}
