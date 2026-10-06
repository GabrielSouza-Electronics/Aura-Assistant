#include <gui/common/MenuLogic.hpp>
#include <gui/common/HandInput.hpp>
#include <math.h>

using namespace HandInput;

/* --- geometria do carrossel, em coordenadas JA ROTACIONADAS -------------
   No sistema nao girado o carrossel ficava em (240, 232) com o anel
   inclinado no eixo Y. Girar 90 a esquerda troca os eixos: o centro vira
   (232, 239) e a inclinacao passa para o eixo X.                         */
static const float STEP      = 2.0f * PI_F / ML_COUNT;   /* 72 graus       */
static const float MENU_CX   = 232.0f;
static const float MENU_CY   = 239.0f;
static const float R_ORBIT   = 176.0f;
static const float CAM_D     = 3.4f;
static const float RING_TILT = 46.0f;

/* --- interacao --------------------------------------------------------- */
/* SPIN_GAIN, HYST, CENTER_*, ACT_* vem de HandInput.hpp (compartilhados
   com o Settings).                                                       */
static const int   DWELL_N   = 48;        /* 0,8 s a 60 Hz; 1,5x faster    */
static const float ACT_DOWN  = -ACT_THRESHOLD;
static const float VIS_K     = 0.10f;
static const float TILT_MAX  = 26.0f;

static const int16_t ICON_SZ[3] = { 36, 48, 64 };


MenuLogic::MenuLogic()
{
    angle = 0.0f;
    sel = 0;
    dwell = 0;
    armed = true;
    centering = false;
    screen = -1;
    msg = 0;
    cancelled = false;
    actHold = 0;
    tilt = 0.0f;
    vis = 0.0f;
    navEvent = 0;
    verticalBack = true;
    spinLock = false;
    for (int i = 0; i < ML_COUNT; i++)
    {
        grow[i] = 0.0f;
    }
}

void MenuLogic::open(int idx)
{
    screen = idx;
    navEvent = 1;
    cancelled = false;
    dwell = 0;
    armed = false;
    msg = 90;
}

void MenuLogic::back()
{
    dwell = 0;
    actHold = 0;
    armed = false;
    if (screen >= 0)
    {
        /* quem saiu empurrando para o lado ainda esta com a mao fora do
           centro: sem a trava o carrossel voltaria girando             */
        spinLock = true;
        screen = -1;
        navEvent = 2;
        msg = 70;
        cancelled = false;
    }
    else
    {
        cancelled = true;
        msg = 70;
    }
}

void MenuLogic::tick(bool handPresent, float handX, float handY, bool click,
                     bool proximity)
{
    vis += ((handPresent ? 1.0f : 0.0f) - vis) * VIS_K;

    if (!handPresent)
    {
        dwell = 0;
        armed = true;
        cancelled = false;
        actHold = 0;
        /* sair do alcance NAO fecha o menu aberto: a pessoa pode estar so
           abaixando a mao. Fechar exige o gesto de voltar.               */
    }
    if (msg > 0)
    {
        msg--;
    }

    const float vy = handPresent && !proximity ? handY : 0.0f;
    if (!handPresent || proximity || screen >= 0 || vy < ACT_DOWN)
    {
        centering = false;
    }
    else if (!click)
    {
        const float magnitude = fabsf(handX);
        if (magnitude <= CENTER_ENTER) centering = true;
        else if (magnitude >= CENTER_EXIT) centering = false;
    }
    if (spinLock && (!handPresent || fabsf(handX) < CENTER_EXIT))
    {
        spinLock = false;
    }
    /* com um menu aberto o carrossel esta escondido: nao gira por baixo */
    const float h = (handPresent && !proximity && !click && !centering && !spinLock &&
                     screen < 0) ? handX : 0.0f;
    angle += SPIN_GAIN * h;
    if (h != 0.0f)
    {
        armed = true;
    }

    bool centered = false;
    if (centering && !click)
    {
        // Align the selected item using the shortest path across the wrap.
        const float fullTurn = 2.0f * PI_F;
        float error = fmodf(-sel * STEP - angle, fullTurn);
        if (error > PI_F) error -= fullTurn;
        if (error < -PI_F) error += fullTurn;
        centered = fabsf(error) <= CENTER_EPS;
        angle += centered ? error : error * CENTER_GAIN;
    }

    /* --- selecao, com histerese --- */
    float raw = -angle / STEP;
    while (raw < 0.0f)
    {
        raw += ML_COUNT;
    }
    raw = fmodf(raw, (float)ML_COUNT);
    int nearIdx = ((int)floorf(raw + 0.5f)) % ML_COUNT;
    if (nearIdx != sel)
    {
        float d = fmodf(raw - sel + ML_COUNT * 1.5f, (float)ML_COUNT)
                  - ML_COUNT * 0.5f;
        if (d < 0.0f) d = -d;
        if (d > HYST)
        {
            sel = nearIdx;
            dwell = 0;
        }
    }

    /* Inputs are normalized by the source (mouse or calibrated ToF).
       Do not select by dwell while the user is requesting back/cancel. */
    if (handPresent && !proximity && centering && centered && vy >= ACT_DOWN)
    {
        dwell++;
    }
    else
    {
        dwell = 0;
    }
    /* A deliberate proximity click can confirm again after cancellation.
       Back/cancel keeps priority when the hand points down. */
    const bool confirmClick = click && handPresent && vy >= ACT_DOWN;
    if (((dwell >= DWELL_N && armed) || confirmClick) && screen < 0)
    {
        open(sel);
    }

    /* --- eixo vertical: inclinar / cancelar --- */
    tilt = vy * TILT_MAX;

    if (handPresent && vy < ACT_DOWN && screen < 0)
    {
        /* Leaving a submenu needs a deliberate 500 ms hold; on the carousel
           the same gesture is only a quick cancel of the dwell.        */
        actHold++;
        if (actHold >= ((screen >= 0) ? BACK_HOLD : ACT_HOLD))
        {
            back();
            actHold = 0;
        }
    }
    else
    {
        actHold = 0;
    }

    for (int i = 0; i < ML_COUNT; i++)
    {
        float t = (i == sel) ? 1.0f : 0.0f;
        grow[i] += (t - grow[i]) * 0.35f;
    }
}

int MenuLogic::getDwellStage() const
{
    if (dwell <= 0 || screen >= 0)
    {
        return -1;
    }
    int st = (dwell * 16) / DWELL_N;
    return (st > 15) ? 15 : st;
}

int MenuLogic::getStateMsg() const
{
    if (cancelled && msg > 0) return 1;
    if (!armed) return 2;
    return 0;
}

void MenuLogic::getIconPose(int i, int16_t& cx, int16_t& cy,
                            int16_t& size, uint8_t& alpha) const
{
    float a = angle + i * STEP + PI_F * 0.5f;
    float x3 = cosf(a), z3 = sinf(a);
    float k = CAM_D / (CAM_D - z3);

    /* Eixos trocados em relacao ao sistema nao girado: o que era a
       coordenada horizontal do anel vira a vertical da tela.            */
    float y = MENU_CY + x3 * R_ORBIT * k;
    float x = MENU_CX + z3 * (RING_TILT + tilt);

    cx = (int16_t)x;
    cy = (int16_t)y;

    float depth = (z3 + 1.0f) * 0.5f;
    float szf = ICON_SZ[0]
              + (ICON_SZ[2] - ICON_SZ[0]) * depth * grow[i]
              + (ICON_SZ[1] - ICON_SZ[0]) * depth * (1.0f - grow[i]);
    int slot = 0;
    float bd = 999.0f;
    for (int s = 0; s < 3; s++)
    {
        float d = szf - ICON_SZ[s];
        if (d < 0.0f) d = -d;
        if (d < bd) { bd = d; slot = s; }
    }
    size = ICON_SZ[slot];

    float dd = depth * depth * (0.6f + 0.4f * depth);   /* ~depth^1.4 */
    float v = vis * vis * (0.7f + 0.3f * vis);
    if (screen >= 0)
    {
        v = 0.0f;       /* com um menu aberto o carrossel sai de cena */
    }
    int al = (int)((70.0f + 185.0f * dd) * v);
    alpha = (uint8_t)((al > 255) ? 255 : (al < 0 ? 0 : al));
}
