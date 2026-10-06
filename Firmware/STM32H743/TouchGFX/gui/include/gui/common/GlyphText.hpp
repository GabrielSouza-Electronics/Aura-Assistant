#ifndef GLYPHTEXT_HPP
#define GLYPHTEXT_HPP

#include <touchgfx/widgets/Widget.hpp>

/*
 * Texto dinamico desenhado com os sprites de SettingsStyleAssets.hpp.
 *
 * Existe porque TextArea nao acompanha o Layout Rotation das imagens: o
 * texto sairia de lado. Aqui cada caractere e um sprite ja girado, e o
 * widget so faz o layout - mesma tecnica do blit do CircuitField.
 *
 * GEOMETRIA (display girado): o texto corre no eixo x LOGICO, que no
 * framebuffer e -y. O widget tem largura SG_TEXT_H e altura igual a largura
 * logica do texto; o topo do widget (y) e a borda DIREITA logica. Por isso
 * alinhar a direita = fixar o y do widget.
 *
 * Preserva maiusculas/minusculas ASCII; caracteres fora do atlas viram '?'.
 * Se nao couber em maxW (px logicos) e cortado com "..".
 */
#define GT_MAX_CHARS 24

class GlyphText : public touchgfx::Widget
{
public:
    GlyphText();

    /* Copia o texto; ajusta largura e altura do widget. Nao invalida:
       o chamador invalida antes e depois, como nos outros widgets.     */
    void setText(const char* text, int16_t maxW);

    void    setAlpha(uint8_t a) { alpha = a; }
    uint8_t getAlpha() const { return alpha; }

    virtual void draw(const touchgfx::Rect& invalidatedArea) const;
    virtual touchgfx::Rect getSolidRect() const { return touchgfx::Rect(); }

private:
    uint8_t glyph[GT_MAX_CHARS];    /* indice em SG_GLYPHS                  */
    int16_t pos[GT_MAX_CHARS];      /* inicio logico da celula              */
    uint8_t count;
    uint8_t alpha;
    int16_t extent;                 /* largura logica total                 */

    int  layout(const char* text, int n, bool dots, int16_t maxW);
};

#endif
