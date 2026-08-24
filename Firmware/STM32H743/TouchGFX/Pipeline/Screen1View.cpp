#include <gui/screen1_screen/Screen1View.hpp>
#include <images/BitmapDatabase.hpp>

using namespace touchgfx;
using namespace aura;

// Quantas estrelas atualizar por frame. Espalhar o custo mantem 60 fps.
static const int STARS_PER_TICK = 12;

// Amplitude do cintilar, em unidades de alpha (0..255).
static const int TWINKLE_AMP = 55;

// Deriva do campo de estrelas: periodo em frames e amplitude em pixels.
static const int DRIFT_PERIOD = 900;
static const int DRIFT_AMP    = 2;

static const uint16_t BMP_GLASS_IDLE = BITMAP_NODE_GLASS_IDLE_ID;
static const uint16_t BMP_GLASS_SEL  = BITMAP_NODE_GLASS_SEL_ID;

static const uint16_t BMP_ICON_IDLE[NUM_NODES] = {
    BITMAP_ICON_TASKS_IDLE_ID,     BITMAP_ICON_EMAIL_IDLE_ID,
    BITMAP_ICON_REMINDERS_IDLE_ID, BITMAP_ICON_ASSISTANT_IDLE_ID,
    BITMAP_ICON_CALENDAR_IDLE_ID,
};
static const uint16_t BMP_ICON_SEL[NUM_NODES] = {
    BITMAP_ICON_TASKS_SEL_ID,     BITMAP_ICON_EMAIL_SEL_ID,
    BITMAP_ICON_REMINDERS_SEL_ID, BITMAP_ICON_ASSISTANT_SEL_ID,
    BITMAP_ICON_CALENDAR_SEL_ID,
};
static const uint16_t BMP_PILL_IDLE[NUM_NODES] = {
    BITMAP_PILL_TASKS_IDLE_ID,     BITMAP_PILL_EMAIL_IDLE_ID,
    BITMAP_PILL_REMINDERS_IDLE_ID, BITMAP_PILL_ASSISTANT_IDLE_ID,
    BITMAP_PILL_CALENDAR_IDLE_ID,
};
static const uint16_t BMP_PILL_SEL[NUM_NODES] = {
    BITMAP_PILL_TASKS_SEL_ID,     BITMAP_PILL_EMAIL_SEL_ID,
    BITMAP_PILL_REMINDERS_SEL_ID, BITMAP_PILL_ASSISTANT_SEL_ID,
    BITMAP_PILL_CALENDAR_SEL_ID,
};

static const Placed POS_PILL_IDLE[NUM_NODES] = {
    POS_PILL_TASKS_IDLE, POS_PILL_EMAIL_IDLE, POS_PILL_REMINDERS_IDLE,
    POS_PILL_ASSISTANT_IDLE, POS_PILL_CALENDAR_IDLE,
};
static const Placed POS_PILL_SEL[NUM_NODES] = {
    POS_PILL_TASKS_SEL, POS_PILL_EMAIL_SEL, POS_PILL_REMINDERS_SEL,
    POS_PILL_ASSISTANT_SEL, POS_PILL_CALENDAR_SEL,
};

// Ordem de rotacao na orbita, sentido horario a partir do topo.
static const int ORBIT_ORDER[NUM_NODES] = {
    NODE_CALENDAR, NODE_EMAIL, NODE_ASSISTANT, NODE_REMINDERS, NODE_TASKS
};

// Seno inteiro barato: 0..255 -> -128..127. Evita float no tick.
static int8_t isin(uint8_t a)
{
    static const int8_t q[65] = {
        0, 3, 6, 9, 12, 15, 18, 21, 24, 28, 31, 34, 37, 40, 43, 46,
        48, 51, 54, 57, 60, 63, 65, 68, 71, 73, 76, 78, 81, 83, 85, 88,
        90, 92, 94, 96, 98, 100, 102, 104, 106, 107, 109, 111, 112, 113,
        115, 116, 117, 118, 120, 121, 122, 122, 123, 124, 125, 125, 126,
        126, 126, 127, 127, 127, 127
    };
    if (a < 64)  { return  q[a]; }
    if (a < 128) { return  q[128 - a]; }
    if (a < 192) { return -q[a - 128]; }
    return -q[256 - a];
}


Screen1View::Screen1View()
    : starCount(0), starCursor(0), selected(NODE_CALENDAR),
      ambient(true), tick(0)
{
}

// ---------------------------------------------------------------- setup

void Screen1View::buildStars()
{
    struct Group { const Instance* inst; int n; uint16_t bmp; };
    const Group groups[] = {
        { INST_STAR_0P45, N_STAR_0P45, BITMAP_STAR_0P45_ID },
        { INST_STAR_0P55, N_STAR_0P55, BITMAP_STAR_0P55_ID },
        { INST_STAR_0P7,  N_STAR_0P7,  BITMAP_STAR_0P7_ID  },
        { INST_STAR_0P85, N_STAR_0P85, BITMAP_STAR_0P85_ID },
        { INST_STAR_1P1,  N_STAR_1P1,  BITMAP_STAR_1P1_ID  },
        { INST_STAR_1P4,  N_STAR_1P4,  BITMAP_STAR_1P4_ID  },
    };

    starCount = 0;
    for (unsigned g = 0; g < sizeof(groups) / sizeof(groups[0]); g++)
    {
        for (int i = 0; i < groups[g].n && starCount < AURA_TOTAL_STARS; i++)
        {
            const Instance& in = groups[g].inst[i];
            Star& s = stars[starCount];

            s.img.setBitmap(Bitmap(groups[g].bmp));
            s.img.setXY(in.x, in.y);
            s.img.setAlpha(in.alpha);

            s.baseX = in.x;
            s.baseY = in.y;
            s.baseAlpha = in.alpha;
            // fase e velocidade derivadas da posicao: distribui o cintilar
            // sem precisar de RNG nem de tabela extra na flash
            s.phase = (uint8_t)((in.x * 7 + in.y * 13) & 0xFF);
            s.speed = (uint8_t)(1 + ((in.x ^ in.y) & 0x03));

            add(s.img);
            starCount++;
        }
    }
}

void Screen1View::setupScreen()
{
    Screen1ViewBase::setupScreen();

    // --- fundo opaco -------------------------------------------------
    background.setBitmap(Bitmap(BITMAP_BG_SPACE_ID));
    background.setXY(POS_BG_SPACE.x, POS_BG_SPACE.y);
    add(background);

    // --- nebulosas ---------------------------------------------------
    const uint16_t nb[4] = { BITMAP_NEBULA_0_ID, BITMAP_NEBULA_1_ID,
                             BITMAP_NEBULA_2_ID, BITMAP_NEBULA_3_ID };
    const Placed nbp[4] = { POS_NEBULA_0, POS_NEBULA_1,
                            POS_NEBULA_2, POS_NEBULA_3 };
    for (int i = 0; i < 4; i++)
    {
        nebula[i].setBitmap(Bitmap(nb[i]));
        nebula[i].setXY(nbp[i].x, nbp[i].y);
        add(nebula[i]);
    }

    // --- estrelas ----------------------------------------------------
    buildStars();

    // --- orbitas -----------------------------------------------------
    const uint16_t ob[6] = { BITMAP_ORBIT_0_ID, BITMAP_ORBIT_1_ID,
                             BITMAP_ORBIT_2_ID, BITMAP_ORBIT_3_ID,
                             BITMAP_ORBIT_4_ID, BITMAP_ORBIT_5_ID };
    const Placed obp[6] = { POS_ORBIT_0, POS_ORBIT_1, POS_ORBIT_2,
                            POS_ORBIT_3, POS_ORBIT_4, POS_ORBIT_5 };
    for (int i = 0; i < 6; i++)
    {
        orbit[i].setBitmap(Bitmap(ob[i]));
        orbit[i].setXY(obp[i].x, obp[i].y);
        add(orbit[i]);
    }

    // --- nucleo ------------------------------------------------------
    struct Simple { Image* w; uint16_t bmp; Placed p; };
    const Simple core[] = {
        { &coreGlow,   BITMAP_CORE_GLOW_ID,   POS_CORE_GLOW   },
        { &coreDiscos, BITMAP_CORE_DISCOS_ID, POS_CORE_DISCOS },
        { &coreAneis,  BITMAP_CORE_ANEIS_ID,  POS_CORE_ANEIS  },
        { &coreBojo,   BITMAP_CORE_BOJO_ID,   POS_CORE_BOJO   },
        { &coreArcos,  BITMAP_CORE_ARCOS_ID,  POS_CORE_ARCOS  },
        { &coreTexto,  BITMAP_CORE_TEXTO_ID,  POS_CORE_TEXTO  },
        { &coreHaste,  BITMAP_CORE_HASTE_ID,  POS_CORE_HASTE  },
    };
    for (unsigned i = 0; i < sizeof(core) / sizeof(core[0]); i++)
    {
        core[i].w->setBitmap(Bitmap(core[i].bmp));
        core[i].w->setXY(core[i].p.x, core[i].p.y);
        add(*core[i].w);
    }

    // --- nos: vidro, icone e pilula ----------------------------------
    // Idle primeiro, depois todos os sel: o selecionado e maior e precisa
    // ficar na frente dos vizinhos para o halo nao ser recortado.
    for (int i = 0; i < NUM_NODES; i++)
    {
        glassIdle[i].setBitmap(Bitmap(BMP_GLASS_IDLE));
        glassIdle[i].setXY(GLASS_IDLE[i].x, GLASS_IDLE[i].y);
        add(glassIdle[i]);

        iconIdle[i].setBitmap(Bitmap(BMP_ICON_IDLE[i]));
        iconIdle[i].setXY(ICON_IDLE[i].x, ICON_IDLE[i].y);
        add(iconIdle[i]);

        pillIdle[i].setBitmap(Bitmap(BMP_PILL_IDLE[i]));
        pillIdle[i].setXY(POS_PILL_IDLE[i].x, POS_PILL_IDLE[i].y);
        add(pillIdle[i]);
    }
    for (int i = 0; i < NUM_NODES; i++)
    {
        glassSel[i].setBitmap(Bitmap(BMP_GLASS_SEL));
        glassSel[i].setXY(GLASS_SEL[i].x, GLASS_SEL[i].y);
        glassSel[i].setVisible(false);
        add(glassSel[i]);

        iconSel[i].setBitmap(Bitmap(BMP_ICON_SEL[i]));
        iconSel[i].setXY(ICON_SEL[i].x, ICON_SEL[i].y);
        iconSel[i].setVisible(false);
        add(iconSel[i]);

        pillSel[i].setBitmap(Bitmap(BMP_PILL_SEL[i]));
        pillSel[i].setXY(POS_PILL_SEL[i].x, POS_PILL_SEL[i].y);
        pillSel[i].setVisible(false);
        add(pillSel[i]);
    }

    // --- header, laterais e rodape -----------------------------------
    const Simple chrome[] = {
        { &hdrAnel,    BITMAP_HDR_ANEL_ID,     POS_HDR_ANEL     },
        { &hdrTitulo,  BITMAP_HDR_TITULO_ID,   POS_HDR_TITULO   },
        { &hdrWifi,    BITMAP_HDR_WIFI_ID,     POS_HDR_WIFI     },
        { &hdrBateria, BITMAP_HDR_BATERIA_ID,  POS_HDR_BATERIA  },
        { &ftrIconeLr, BITMAP_FTR_ICONE_LR_ID, POS_FTR_ICONE_LR },
        { &ftrTextoLr, BITMAP_FTR_TEXTO_LR_ID, POS_FTR_TEXTO_LR },
        { &ftrIconeUd, BITMAP_FTR_ICONE_UD_ID, POS_FTR_ICONE_UD },
        { &ftrTextoUd, BITMAP_FTR_TEXTO_UD_ID, POS_FTR_TEXTO_UD },
        { &ftrIconeHd, BITMAP_FTR_ICONE_HD_ID, POS_FTR_ICONE_HD },
        { &ftrTextoHd, BITMAP_FTR_TEXTO_HD_ID, POS_FTR_TEXTO_HD },
    };
    for (unsigned i = 0; i < sizeof(chrome) / sizeof(chrome[0]); i++)
    {
        chrome[i].w->setBitmap(Bitmap(chrome[i].bmp));
        chrome[i].w->setXY(chrome[i].p.x, chrome[i].p.y);
        add(*chrome[i].w);
    }

    for (int i = 0; i < N_SIDE_ARROW && i < 2; i++)
    {
        sideArrow[i].setBitmap(Bitmap(BITMAP_SIDE_ARROW_ID));
        sideArrow[i].setXY(INST_SIDE_ARROW[i].x, INST_SIDE_ARROW[i].y);
        add(sideArrow[i]);
    }
    for (int i = 0; i < N_FTR_DIVISOR && i < 2; i++)
    {
        divisor[i].setBitmap(Bitmap(BITMAP_FTR_DIVISOR_ID));
        divisor[i].setXY(INST_FTR_DIVISOR[i].x, INST_FTR_DIVISOR[i].y);
        add(divisor[i]);
    }
    dotAtivo.setBitmap(Bitmap(BITMAP_DOT_ATIVO_ID));
    dotAtivo.setXY(INST_DOT_ATIVO[0].x, INST_DOT_ATIVO[0].y);
    add(dotAtivo);
    for (int i = 0; i < N_DOT_INATIVO && i < 4; i++)
    {
        dotInativo[i].setBitmap(Bitmap(BITMAP_DOT_INATIVO_ID));
        dotInativo[i].setXY(INST_DOT_INATIVO[i].x, INST_DOT_INATIVO[i].y);
        add(dotInativo[i]);
    }

    applySelection();
}

void Screen1View::tearDownScreen()
{
    Screen1ViewBase::tearDownScreen();
}

// ------------------------------------------------------------ animacao

void Screen1View::animateStars()
{
    // Atualiza so uma fatia por frame. Cada estrela e revisitada a cada
    // (starCount / STARS_PER_TICK) frames, o que da um cintilar organico
    // e mantem o custo de invalidacao baixo.
    for (int k = 0; k < STARS_PER_TICK && starCount > 0; k++)
    {
        Star& s = stars[starCursor];

        s.phase = (uint8_t)(s.phase + s.speed);
        const int wobble = (isin(s.phase) * TWINKLE_AMP) / 128;
        int a = (int)s.baseAlpha + wobble;
        if (a < 20)  { a = 20; }
        if (a > 255) { a = 255; }
        s.img.setAlpha((uint8_t)a);

        // deriva lenta: todo o campo respira junto, alguns pixels
        const uint8_t dphase = (uint8_t)((tick * 256 / DRIFT_PERIOD)
                                         + (s.baseX & 0x3F));
        const int dx = (isin(dphase) * DRIFT_AMP) / 128;
        const int dy = (isin((uint8_t)(dphase + 64)) * DRIFT_AMP) / 128;
        s.img.setXY((int16_t)(s.baseX + dx), (int16_t)(s.baseY + dy));

        s.img.invalidate();
        starCursor = (starCursor + 1) % starCount;
    }
}

void Screen1View::animateAmbient()
{
    // Nebulosas respirando: periodos primos entre si para nunca sincronizar
    static const uint8_t nbSpeed[4] = { 1, 1, 2, 1 };
    static const uint8_t nbBase[4]  = { 220, 210, 200, 215 };
    for (int i = 0; i < 4; i++)
    {
        const uint8_t ph = (uint8_t)((tick * nbSpeed[i] / 4) + i * 61);
        int a = nbBase[i] + (isin(ph) * 30) / 128;
        if (a < 0)   { a = 0; }
        if (a > 255) { a = 255; }
        if (nebula[i].getAlpha() != (uint8_t)a)
        {
            nebula[i].setAlpha((uint8_t)a);
            nebula[i].invalidate();
        }
    }

    // Orbitas com brilho pulsante, cada uma na sua fase
    for (int i = 0; i < 6; i++)
    {
        const uint8_t ph = (uint8_t)((tick / 3) + i * 40);
        int a = 200 + (isin(ph) * 45) / 128;
        if (a < 0)   { a = 0; }
        if (a > 255) { a = 255; }
        if (orbit[i].getAlpha() != (uint8_t)a)
        {
            orbit[i].setAlpha((uint8_t)a);
            orbit[i].invalidate();
        }
    }

    // Nucleo respirando devagar
    const uint8_t ph = (uint8_t)(tick / 5);
    int a = 215 + (isin(ph) * 40) / 128;
    if (a < 0)   { a = 0; }
    if (a > 255) { a = 255; }
    if (coreGlow.getAlpha() != (uint8_t)a)
    {
        coreGlow.setAlpha((uint8_t)a);
        coreGlow.invalidate();
    }
}

void Screen1View::handleTickEvent()
{
    tick++;
    animateStars();

    // O ambiente muda devagar: atualizar a cada 4 frames e suficiente
    // e corta o custo por quatro.
    if (ambient && (tick & 0x03) == 0)
    {
        animateAmbient();
    }
}

// ------------------------------------------------------------ selecao

void Screen1View::applySelection()
{
    for (int i = 0; i < NUM_NODES; i++)
    {
        const bool on = (i == selected);
        glassIdle[i].setVisible(!on);
        iconIdle[i].setVisible(!on);
        pillIdle[i].setVisible(!on);
        glassSel[i].setVisible(on);
        iconSel[i].setVisible(on);
        pillSel[i].setVisible(on);
    }
    invalidate();
}

void Screen1View::selectNode(int index)
{
    if (index < 0 || index >= NUM_NODES || index == selected)
    {
        return;
    }
    selected = index;
    applySelection();
}

void Screen1View::rotateSelection(int delta)
{
    int pos = 0;
    for (int i = 0; i < NUM_NODES; i++)
    {
        if (ORBIT_ORDER[i] == selected)
        {
            pos = i;
            break;
        }
    }
    pos = (pos + delta + NUM_NODES) % NUM_NODES;
    selectNode(ORBIT_ORDER[pos]);
}

int Screen1View::hitTest(int16_t x, int16_t y) const
{
    // Raio generoso em volta do centro de cada no: mais tolerante ao dedo
    // do que a bbox do sprite, e sem ambiguidade entre vizinhos.
    static const int R2 = 44 * 44;
    for (int i = 0; i < NUM_NODES; i++)
    {
        const int dx = x - NODE_CENTER[i].x;
        const int dy = y - NODE_CENTER[i].y;
        if (dx * dx + dy * dy <= R2)
        {
            return i;
        }
    }
    return -1;
}

void Screen1View::handleClickEvent(const ClickEvent& evt)
{
    Screen1ViewBase::handleClickEvent(evt);

    if (evt.getType() != ClickEvent::RELEASED)
    {
        return;
    }
    const int hit = hitTest(evt.getX(), evt.getY());
    if (hit >= 0)
    {
        selectNode(hit);
    }
}
