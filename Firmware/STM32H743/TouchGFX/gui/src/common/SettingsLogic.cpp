#include <gui/common/SettingsLogic.hpp>
#include <gui/common/HandInput.hpp>
#include <gui/common/MenuLogic.hpp>
#include <math.h>

using namespace HandInput;

/* Mesmas constantes do preview em aura_assets/gen/gen_settings.py: se
   mudar aqui, mude la para o GIF continuar representativo.             */
static const int   REVEAL_DELAY = 4;       /* ticks entre linhas          */
static const int   REVEAL_DUR   = 16;      /* ticks de cada entrada       */
static const int   HEADER_DUR   = 14;
static const float SLIDE_PX     = 44.0f;
static const float FADE_OUT_K   = 1.0f / 8.0f;
static const int   GLOW_PERIOD  = 90;      /* 1,5 s a 60 Hz               */
static const uint16_t T_MAX     = REVEAL_DELAY * SL_ROWS + REVEAL_DUR + HEADER_DUR;

/* Velocidade do carrossel convertida de radianos para "itens": com a mao
   no mesmo lugar, a lista anda tantas linhas por segundo quanto o
   carrossel anda icones. CENTER_EPS idem.                               */
static const float CAROUSEL_STEP = 2.0f * PI_F / ML_COUNT;
static const float ITEM_RATE     = -SPIN_GAIN / CAROUSEL_STEP;   /* ~0.044 */
static const float ITEM_EPS      = CENTER_EPS / CAROUSEL_STEP;

static const float EDIT_K       = 0.20f;
static const float EDIT_SHIFT   = 10.0f;   /* linha em ajuste anda p/ dir */
static const float LEAN_X       = 8.0f;    /* inclinacao maxima, px       */
static const float LEAN_Y       = 5.0f;
static const float LEAN_K       = 0.30f;
static const float BUMP_PX      = 7.0f;
static const float BUMP_DECAY   = 0.80f;
static const uint8_t FLASH_TICKS = 14;
static const float DIM_OTHERS   = 0.45f;   /* linhas fora de foco no ajuste */

static float smooth(float x)
{
    if (x <= 0.0f) return 0.0f;
    if (x >= 1.0f) return 1.0f;
    return x * x * (3.0f - 2.0f * x);
}

SettingsLogic::SettingsLogic()
{
    t = 0;
    glowPhase = 0;
    fade = 0.0f;
    editK = 0.0f;
    bump = 0.0f;
    flash = 0;
    pos = 0.0f;
    focus = 0;
    centering = true;
    atEdge = false;
    editing = false;
    vpos = 0.0f;
    vlast = 0.0f;
    actHold = 0;
    actDir = 0;
    actArmed = true;
    leanX = 0.0f;
    leanY = 0.0f;
    reqPending = false;
    reqItem = 0;
    reqDelta = 0;
    exitReq = false;
    feedback = Feedback::None;
}

void SettingsLogic::request(int8_t delta)
{
    reqPending = true;
    reqItem = (int8_t)focus;
    reqDelta = delta;
    flash = FLASH_TICKS;
    feedback = Feedback::Change;
}

void SettingsLogic::action(int dir)
{
    if (dir > 0)
    {
        if (!editing && focus != 0) /* Wi-Fi is a read-only connection label. */
        {
            editing = true;
            vpos = vlast = 0.0f;
            feedback = Feedback::Enter;
        }
    }
    else if (editing)
    {
        editing = false;
        feedback = Feedback::Leave;
    }
    else
    {
        exitReq = true;     /* o som de saida vem da MenuLogic */
    }
}

void SettingsLogic::tick(bool active, bool handPresent, float handX,
                         float handY, bool click)
{
    /* --- animacao de entrada/saida ------------------------------------ */
    if (active)
    {
        fade = 1.0f;
        if (t < T_MAX) t++;
    }
    else
    {
        fade -= FADE_OUT_K;
        if (fade <= 0.0f)
        {
            fade = 0.0f;
            t = 0;          /* proxima abertura refaz a cascata */
        }
        editing = false;
    }
    glowPhase = (uint16_t)((glowPhase + 1) % GLOW_PERIOD);

    /* A entrada so vale com o menu aberto e a cascata terminada; ate la a
       mao que abriu o menu ainda esta no gesto do carrossel.           */
    const bool live = active && getHeaderProgress() >= 1.0f;
    const bool present = live && handPresent;
    const float x = present ? handX : 0.0f;
    const float y = present ? handY : 0.0f;

    /* --- horizontal: entrar / voltar (mesmo ACT_* do carrossel) ------- */
    const bool horizontal = present && fabsf(x) >= ACT_THRESHOLD &&
                            fabsf(x) >= fabsf(y);
    const int dir = (x > 0.0f) ? 1 : -1;
    /* At 5 Hz the sensor can skip the neutral position between right and
       left. A reversal rearms; holding the same direction cannot exit twice. */
    if (horizontal && dir != actDir) actArmed = true;
    if (!horizontal)
    {
        actHold = 0;
        if (fabsf(x) < CENTER_EXIT) actArmed = true;
    }
    else if (actArmed)
    {
        if (dir != actDir) actHold = 0;
        actDir = dir;
        if (++actHold >= ACT_HOLD)
        {
            action(dir);
            actHold = 0;
            actArmed = false;
        }
    }
    /* Back has priority over a simultaneous proximity click. */
    if (live && click && handPresent && !horizontal) action(1);

    /* --- vertical: mesmo joystick do giro do carrossel ---------------- */
    const float vy = horizontal ? 0.0f : y;
    if (!present) centering = true;
    else if (fabsf(vy) <= CENTER_ENTER) centering = true;
    else if (fabsf(vy) >= CENTER_EXIT) centering = false;

    if (!editing)
    {
        if (!centering)
        {
            pos -= ITEM_RATE * vy;            /* mao para cima = linha acima */
        }
        const float lo = 0.0f;
        const float hi = (float)(SL_ROWS - 1);
        if (pos < lo || pos > hi)
        {
            pos = (pos < lo) ? lo : hi;
            if (!atEdge)
            {
                atEdge = true;
                bump = BUMP_PX * ((pos == lo) ? -1.0f : 1.0f);
                feedback = Feedback::Bump;
            }
        }
        else if (fabsf(pos - focus) < 0.5f)
        {
            atEdge = false;
        }

        if (centering)
        {
            const float error = (float)focus - pos;
            pos += (fabsf(error) <= ITEM_EPS) ? error : error * CENTER_GAIN;
        }

        /* selecao com a mesma histerese do carrossel */
        const int nearest = (int)floorf(pos + 0.5f);
        if (nearest != focus && fabsf(pos - focus) > HYST)
        {
            focus = nearest;
            feedback = Feedback::Move;
        }
    }
    else
    {
        if (!centering)
        {
            vpos += ITEM_RATE * vy;           /* mao para cima = +1 */
        }
        else
        {
            vpos += (vlast - vpos) * CENTER_GAIN;
        }
        if (vpos - vlast > HYST)
        {
            vlast += 1.0f;
            request(1);
        }
        else if (vpos - vlast < -HYST)
        {
            vlast -= 1.0f;
            request(-1);
        }
    }

    /* --- animacao ------------------------------------------------------ */
    editK += ((editing ? 1.0f : 0.0f) - editK) * EDIT_K;
    leanX += (x - leanX) * LEAN_K;
    leanY += ((editing ? vy : 0.0f) - leanY) * LEAN_K;
    bump *= BUMP_DECAY;
    if (fabsf(bump) < 0.3f) bump = 0.0f;
    if (flash > 0) flash--;
}

float SettingsLogic::reveal(int i) const
{
    const float r = smooth((float)(t - REVEAL_DELAY * i) / REVEAL_DUR);
    return r * fade;
}

uint8_t SettingsLogic::getRowAlpha(int i) const
{
    float k = reveal(i);
    if (i != focus) k *= 1.0f - (1.0f - DIM_OTHERS) * editK;
    return (uint8_t)(255.0f * k);
}

int16_t SettingsLogic::getRowSlide(int i) const
{
    /* so a entrada desliza; na saida as linhas apenas somem no lugar */
    const float r = smooth((float)(t - REVEAL_DELAY * i) / REVEAL_DUR);
    const float dir = (i % 2 == 0) ? 1.0f : -1.0f;
    return (int16_t)((1.0f - r) * SLIDE_PX * dir);
}

float SettingsLogic::getHeaderProgress() const
{
    return smooth((float)t / HEADER_DUR);
}

float SettingsLogic::getFocusOffsetX() const
{
    /* a linha inclina para o lado da mao: da para ver a acao "carregando"
       antes de disparar. No ajuste so a esquerda faz algo.             */
    float lean = leanX;
    if (editing && lean > 0.0f) lean = 0.0f;
    return lean * LEAN_X + editK * EDIT_SHIFT;
}

float SettingsLogic::getFocusOffsetY() const
{
    /* y logico cresce para BAIXO; mao para cima e y positivo */
    return -leanY * LEAN_Y + bump;
}

uint8_t SettingsLogic::getGlowAlpha() const
{
    float k = 0.5f - 0.5f * cosf(6.2831853f * glowPhase / GLOW_PERIOD);
    k += (1.0f - k) * editK;                  /* no ajuste o halo fica aceso */
    k += (1.0f - k) * (flash / (float)FLASH_TICKS);
    return (uint8_t)((150.0f + 105.0f * k) * fade);
}

uint8_t SettingsLogic::getValueAlpha(int i) const
{
    float k = 0.82f;
    if (i == focus)
    {
        k += 0.18f * editK;
        k += (1.0f - k) * (flash / (float)FLASH_TICKS);
    }
    return (uint8_t)(getRowAlpha(i) * k);
}

bool SettingsLogic::takeRequest(int8_t& item, int8_t& delta)
{
    if (!reqPending) return false;
    reqPending = false;
    item = reqItem;
    delta = reqDelta;
    return true;
}
