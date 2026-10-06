#ifndef SETTINGSLOGIC_HPP
#define SETTINGSLOGIC_HPP

#include <stdint.h>
#include <gui/common/SettingsLayout.hpp>

/*
 * Navegacao, animacao e feedback do menu SETTINGS. NAO DESENHA NADA - mesmo
 * papel da MenuLogic: calcula estado, a View aplica nos widgets.
 *
 * ENTRADA: as MESMAS regras do carrossel (HandInput.hpp). A posicao da mao
 * em relacao ao centro do sensor e um joystick:
 *
 *   Lista:   mao para CIMA / BAIXO  -> a lista anda para cima / baixo, com
 *                                      velocidade proporcional (como o giro
 *                                      do carrossel); mao no centro para e
 *                                      alinha na linha mais proxima
 *            mao na DIREITA         -> entra na linha (modo ajuste)
 *            ToF <30 mm por 2 s    -> volta ao carrossel (global)
 *   Ajuste:  mao para CIMA / BAIXO  -> +1 / -1 no valor, mesma velocidade
 *            mao na ESQUERDA        -> volta para a lista
 *   Direita/esquerda disparam apos ACT_HOLD ticks alem de ACT_THRESHOLD, e
 *   rearmam no centro (CENTER_EXIT) ou ao inverter o sentido horizontal -
 *   segurar a esquerda nao sai de dois niveis de uma vez.
 *   Aproximacao curta seguida de retirada (ToF) = direita.
 *
 * O menu nao muda valores: emite PEDIDOS (item, +1/-1) e quem e dono do
 * valor (App) aplica e publica o texto novo.
 */
class SettingsLogic
{
public:
    enum class Feedback : uint8_t { None, Move, Enter, Leave, Change, Bump };

    SettingsLogic();

    /* Uma vez por tick. active = menu Settings aberto. */
    void tick(bool active, bool handPresent, float handX, float handY,
              bool click);

    /* --- visual --------------------------------------------------------- */
    bool    isVisible() const { return fade > 0.0f; }
    uint8_t getRowAlpha(int i) const;
    int16_t getRowSlide(int i) const;       /* entrada, x LOGICO em px     */
    float   getHeaderProgress() const;      /* 0..1: titulo sobe ao topo   */
    int     getFocus() const { return focus; }
    bool    isEditing() const { return editing; }
    float   getGlowRow() const { return pos; }   /* continuo, como o giro */
    uint8_t getGlowAlpha() const;
    /* deslocamento LOGICO da linha em foco: inclinacao + batida + ajuste */
    float   getFocusOffsetX() const;
    float   getFocusOffsetY() const;
    uint8_t getValueAlpha(int i) const;

    /* --- eventos (cada um devolve uma vez e limpa) ---------------------- */
    bool     takeRequest(int8_t& item, int8_t& delta);
    bool     takeExit() { bool e = exitReq; exitReq = false; return e; }
    Feedback takeFeedback() { Feedback f = feedback; feedback = Feedback::None; return f; }

private:
    /* animacao */
    uint16_t t;
    uint16_t glowPhase;
    float    fade;
    float    editK;
    float    bump;
    uint8_t  flash;

    /* lista: mesmo modelo do angulo do carrossel, em unidades de linha */
    float    pos;
    int      focus;
    bool     centering;
    bool     atEdge;

    /* ajuste: acumulador com a mesma histerese */
    bool     editing;
    float    vpos;
    float    vlast;

    /* acao horizontal (entrar / voltar) */
    int      actHold;
    int      actDir;
    bool     actArmed;

    float    leanX;
    float    leanY;

    bool     reqPending;
    int8_t   reqItem;
    int8_t   reqDelta;
    bool     exitReq;
    Feedback feedback;

    float reveal(int i) const;
    void  action(int dir);
    void  request(int8_t delta);
};

#endif
