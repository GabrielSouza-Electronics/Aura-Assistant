#ifndef SCREEN1VIEW_HPP
#define SCREEN1VIEW_HPP

#include <gui_generated/screen1_screen/Screen1ViewBase.hpp>
#include <gui/screen1_screen/Screen1Presenter.hpp>
#include <touchgfx/widgets/Image.hpp>

#include "AuraAtoms.hpp"   // gerado por prep_aura_atoms.py

/*
 * Menu AURA em orbita, montado a partir de sprites atomicos.
 *
 * Nada e desenhado como tela cheia. Cada elemento e um widget proprio com
 * alpha e posicao independentes, o que permite:
 *   - estrelas cintilando em fases diferentes
 *   - deriva sub-pixel lenta do campo de estrelas
 *   - nebulosas respirando em opacidade
 *   - orbitas com brilho pulsante
 *   - trocar o no selecionado sem nenhum bitmap novo
 *
 * Custo de redesenho: o tick atualiza apenas uma fatia das estrelas por
 * frame (STARS_PER_TICK). Invalidar 99 widgets todo frame derrubaria o
 * frame rate; invalidar ~12 e imperceptivel e mantem 60 fps.
 */

// numero total de estrelas soltas, somando todos os tamanhos
#define AURA_TOTAL_STARS (aura::N_STAR_0P45 + aura::N_STAR_0P55 + \
                          aura::N_STAR_0P7  + aura::N_STAR_0P85 + \
                          aura::N_STAR_1P1  + aura::N_STAR_1P4)

class Screen1View : public Screen1ViewBase
{
public:
    Screen1View();
    virtual ~Screen1View() {}

    virtual void setupScreen();
    virtual void tearDownScreen();
    virtual void handleTickEvent();
    virtual void handleClickEvent(const touchgfx::ClickEvent& evt);

    void selectNode(int index);
    void rotateSelection(int delta);
    int  getSelected() const { return selected; }

    /** Liga/desliga as animacoes de fundo (economia de CPU). */
    void setAmbientAnimation(bool on) { ambient = on; }

protected:
    // ---- fundo ------------------------------------------------------
    touchgfx::Image background;
    touchgfx::Image nebula[4];
    touchgfx::Image orbit[6];

    // ---- estrelas ---------------------------------------------------
    struct Star
    {
        touchgfx::Image img;
        int16_t  baseX, baseY;   // posicao original
        uint8_t  baseAlpha;      // brilho de repouso
        uint8_t  phase;          // fase do cintilar, 0..255
        uint8_t  speed;          // velocidade do cintilar
    };
    Star stars[AURA_TOTAL_STARS];
    int  starCount;
    int  starCursor;             // qual fatia atualizar neste frame

    // ---- nucleo -----------------------------------------------------
    touchgfx::Image coreGlow, coreDiscos, coreAneis, coreBojo;
    touchgfx::Image coreArcos, coreTexto, coreHaste;

    // ---- nos --------------------------------------------------------
    touchgfx::Image glassIdle[aura::NUM_NODES];
    touchgfx::Image glassSel[aura::NUM_NODES];
    touchgfx::Image iconIdle[aura::NUM_NODES];
    touchgfx::Image iconSel[aura::NUM_NODES];
    touchgfx::Image pillIdle[aura::NUM_NODES];
    touchgfx::Image pillSel[aura::NUM_NODES];

    // ---- cromo ------------------------------------------------------
    touchgfx::Image hdrAnel, hdrTitulo, hdrWifi, hdrBateria;
    touchgfx::Image sideArrow[2];
    touchgfx::Image ftrIconeLr, ftrTextoLr, ftrIconeUd, ftrTextoUd;
    touchgfx::Image ftrIconeHd, ftrTextoHd;
    touchgfx::Image divisor[2];
    touchgfx::Image dotAtivo, dotInativo[4];

    int      selected;
    bool     ambient;
    uint32_t tick;

    void buildStars();
    void applySelection();
    void animateStars();
    void animateAmbient();
    int  hitTest(int16_t x, int16_t y) const;
};

#endif // SCREEN1VIEW_HPP
