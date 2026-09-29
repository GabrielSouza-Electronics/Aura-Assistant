#include <gui/screen1_screen/Screen1View.hpp>
#include <BitmapDatabase.hpp>
#include <touchgfx/Color.hpp>

using namespace touchgfx;

/* Same curve as aura_assets/gen/gen_hero.py:
   round(255 * (0.5 - 0.5 * cos(2 * pi * frame / 24))). */
static const uint8_t HERO_BREATH[] = {
    0, 4, 17, 37, 64, 95, 128, 160, 191, 218, 238, 251,
    255, 251, 238, 218, 191, 160, 128, 95, 64, 37, 17, 4
};
static_assert(sizeof(HERO_BREATH) == BITMAP_HERO_23_ID - BITMAP_HERO_00_ID + 1,
              "Hero breath curve must match the animation frames");

/* Posicoes fixas, em coordenadas JA ROTACIONADAS. Sao as mesmas da tabela
   do LEIA-ME; se voce mover um widget no Designer, ajuste aqui tambem
   apenas se o codigo o reposiciona (os icones e o anel).                  */
static const int16_t LBL_CY   = 239;   /* centro vertical dos rotulos      */
static const int16_t LBL_CX   = 372;   /* rotulo do carrossel              */
static const int16_t LBL_MENU = 236;   /* titulo do menu aberto            */
static const int16_t MSG_CX   = 418;

/* rotulos: id, largura, altura (as imagens estao giradas, entao a largura
   do PNG e a altura do texto)                                             */
static const uint16_t LBL_ID[5] = {
    BITMAP_LBL_SETTINGS_ID, BITMAP_LBL_TASKS_ID, BITMAP_LBL_REMINDERS_ID,
    BITMAP_LBL_CALENDAR_ID, BITMAP_LBL_CHAT_ID
};
/* Os PNGs estao girados, entao a LARGURA do arquivo e 29 (a altura do
   texto) e a ALTURA e o comprimento da palavra. Cada rotulo precisa ser
   recentrado porque as palavras tem comprimentos diferentes.            */
static const int16_t LBL_W[5] = { 29, 29, 29, 29, 29 };
static const int16_t LBL_H[5] = { 135, 91, 157, 146, 79 };

static const uint16_t MSG_ID[3] = {
    BITMAP_MSG_HOLD_ID, BITMAP_MSG_CANCELLED_ID, BITMAP_MSG_MOVE_ID
};
static const int16_t MSG_H[3] = { 140, 99, 167 };
static const int16_t MSG_BACK_H = 205;

/* textos da tela de espera, ja girados: largura e a altura do texto */
static const int16_t SB_READY_W = 39,  SB_READY_H = 121;
static const int16_t SB_WAVE_W  = 17,  SB_WAVE_H  = 134;

static const uint16_t WIFI_ID[4] = {
    BITMAP_WIFI_0_ID, BITMAP_WIFI_1_ID, BITMAP_WIFI_2_ID, BITMAP_WIFI_3_ID
};

static const uint16_t ICON_ID[5][3] = {
    { BITMAP_SETTINGS_36_ID,  BITMAP_SETTINGS_48_ID,  BITMAP_SETTINGS_64_ID  },
    { BITMAP_TASKS_36_ID,     BITMAP_TASKS_48_ID,     BITMAP_TASKS_64_ID     },
    { BITMAP_REMINDERS_36_ID, BITMAP_REMINDERS_48_ID, BITMAP_REMINDERS_64_ID },
    { BITMAP_CALENDAR_36_ID,  BITMAP_CALENDAR_48_ID,  BITMAP_CALENDAR_64_ID  },
    { BITMAP_CHAT_36_ID,      BITMAP_CHAT_48_ID,      BITMAP_CHAT_64_ID      }
};

static int16_t iconSlot(int16_t sz)
{
    return (sz <= 40) ? 0 : ((sz <= 56) ? 1 : 2);
}


Screen1View::Screen1View()
{
    handPresent = false;
    previousHandPresent = false;
    handX = handY = 0.0f;
    wifiLevel = 3;
    battLevel = 100;
    charging = false;
    lastScreen = -1;
    for (int i = 0; i < 5; i++)
    {
        lastIconId[i] = 0;
    }
}

void Screen1View::setupScreen()
{
    Screen1ViewBase::setupScreen();

    /* O campo de particulas entra logo DEPOIS do fundo da placa: fica sobre
       ela e sob todo o resto. insert() evita ter que reordenar a pilha de
       widgets no Designer.                                               */
    field.setPosition(0, 0, 480, 480);
    insert(&board, field);

    /* AnimatedImage nao anima sozinho: precisa de startAnimation.
         reverse = false  -> toca do primeiro ao ultimo frame
         reset   = true   -> comeca do frame 0
         loop    = true   -> repete indefinidamente
       Se voce marcar "Auto start" no Designer estas duas chamadas viram
       redundantes, mas nao atrapalham.                                  */
    /* 24 frames x 3 ticks: a 72-tick breath cycle.
       The LED ring follows the current Hero frame through the Model. */
    hero.setUpdateTicksInterval(3);
    hero.startAnimation(false, true, true);
    divSpark.startAnimation(false, true, true);

    divLine.setXY(314, 109);
    divSpark.setX(305);
    divLine.setAlpha(190);
    divSpark.setAlpha(190);
    divLine.setVisible(true);
    divSpark.setVisible(true);

    /* --- textos da tela de espera ---
       Ficam onde o rotulo do carrossel aparece depois; um substitui o
       outro conforme a mao chega.                                      */
    sbReady.setBitmap(Bitmap(BITMAP_SB_READY_ID));
    sbReady.setXY((int16_t)(292 - SB_READY_W / 2),
                  (int16_t)(239 - SB_READY_H / 2));
    sbWave.setBitmap(Bitmap(BITMAP_SB_WAVE_ID));
    sbWave.setXY((int16_t)(348 - SB_WAVE_W / 2),
                 (int16_t)(239 - SB_WAVE_H / 2));

    applyStatus();
    heroHomeX = hero.getX();
    heroHomeY = hero.getY();
    startupStage = StartupStage::Rise;
    startupTicks = 0;
    setHomeVisible(false);
    // Assets are rotated: physical bottom is framebuffer +X.
    hero.setXY(480, (480 - hero.getHeight()) / 2);
    hero.setAlpha(255);
}

void Screen1View::setHomeVisible(bool visible)
{
    Drawable* widgets[] = {
        &board, &field, &icon0, &icon1, &icon2, &icon3, &icon4,
        &selRing, &lblOption, &divLine, &divSpark, &msgState,
        &logoStatus, &wifiIcon, &battFill, &battFrame, &sbReady, &sbWave
    };
    for (Drawable* widget : widgets)
    {
        widget->setVisible(visible);
    }
    invalidate();
}

bool Screen1View::tickStartup()
{
    if (startupStage == StartupStage::Done) return false;
    // Discard gestures collected during bring-up, including proximity clicks.
    handPresent = false;
    previousHandPresent = false;
    handClickPending = false;
    const int16_t centerX = (480 - hero.getWidth()) / 2;
    const int16_t centerY = (480 - hero.getHeight()) / 2;
    ++startupTicks;
    if (startupStage == StartupStage::Wait)
    {
        // Minimum centered hold; on hardware also wait for successful ToF frames.
        if (startupTicks >= 45 && presenter->startupReady())
        {
            startupStage = StartupStage::Dock;
            startupTicks = 0;
        }
        else if (startupTicks >= 45)
        {
            startupTicks = 45; // Indefinite sensor wait without counter overflow.
        }
        return true;
    }
    const uint16_t duration = (startupStage == StartupStage::Rise) ? 45 : 36;
    const float t = (float)startupTicks / duration;
    const float ease = t * t * (3.0f - 2.0f * t);
    const int16_t fromX = (startupStage == StartupStage::Rise) ? 480 : centerX;
    const int16_t toX = (startupStage == StartupStage::Rise) ? centerX : heroHomeX;
    const int16_t toY = (startupStage == StartupStage::Rise) ? centerY : heroHomeY;
    hero.invalidate();
    hero.setXY((int16_t)(fromX + (toX - fromX) * ease),
               (int16_t)(centerY + (toY - centerY) * ease));
    hero.invalidate();
    if (startupTicks >= duration)
    {
        startupTicks = 0;
        if (startupStage == StartupStage::Rise)
        {
            startupStage = StartupStage::Wait;
        }
        else
        {
            startupStage = StartupStage::Done;
            setHomeVisible(true);
            presenter->completeStartup();
            return false; // Apply the idle layout before this frame is rendered.
        }
    }
    return true;
}

void Screen1View::tearDownScreen()
{
    Screen1ViewBase::tearDownScreen();
}

void Screen1View::setWifiLevel(int lvl)
{
    int v = (lvl < 0) ? 0 : ((lvl > 3) ? 3 : lvl);
    if (v != wifiLevel) { wifiLevel = v; applyStatus(); }
}

void Screen1View::setBatteryLevel(int pct)
{
    int v = (pct < 0) ? 0 : ((pct > 100) ? 100 : pct);
    if (v != battLevel) { battLevel = v; applyStatus(); }
}

void Screen1View::setChargerStatus(bool chg)
{
    if (chg != charging) { charging = chg; applyStatus(); }
}

void Screen1View::applyStatus()
{
    wifiIcon.setBitmap(Bitmap(WIFI_ID[wifiLevel]));
    wifiIcon.invalidate();

    /* A bateria esta girada: o preenchimento cresce na VERTICAL.
       Cor troca em tres limiares, nao dois: o aviso chega antes de o
       usuario precisar agir.                                            */
    battFill.invalidate();
    int h = (24 * battLevel) / 100;
    battFill.setHeight((int16_t)h);
    battFill.setY((int16_t)(180 + (24 - h)));
    if (charging)
        battFill.setColor(Color::getColorFromRGB(0x78, 0xF5, 0xA5));
    else if (battLevel <= 12)
        battFill.setColor(Color::getColorFromRGB(0xFF, 0x60, 0x60));
    else if (battLevel <= 30)
        battFill.setColor(Color::getColorFromRGB(0xFF, 0xB2, 0x40));
    else
        battFill.setColor(Color::getColorFromRGB(0x56, 0xD2, 0xFF));
    battFill.invalidate();
}

void Screen1View::handleTickEvent()
{
    const unsigned heroFrame = hero.getBitmap().getId() - BITMAP_HERO_00_ID;
    if (heroFrame < sizeof(HERO_BREATH))
    {
        presenter->setHeroBreath(HERO_BREATH[heroFrame]);
    }

    if (tickStartup()) return;

    const int previousScreen = menu.getScreen();
    const int previousItem = menu.getSelected();
    menu.tick(handPresent, handX, handY, handClickPending);
    handClickPending = false;
    const MenuSound sound = menuSoundForTransition(previousScreen, previousItem,
        menu.getScreen(), menu.getSelected(), previousHandPresent, handPresent);
    previousHandPresent = handPresent;
    if (sound != MenuSound::None)
        presenter->playMenuSound(sound);

    /* a esfera acompanha o carrossel com fator MENOR. Girando junto, ela e
       os icones formariam um bloco rigido; a diferenca de velocidade e o
       que da a leitura de duas camadas.                                  */
    field.tick(menu.getVisibility(), -menu.getAngle() * 0.45f * 57.2958f);

    const int screen = menu.getScreen();
    const bool inMenu = (screen >= 0);
    presenter->setCarouselLED(menu.getAngle(),
        inMenu ? 0U : (uint8_t)(255.0f * menu.getVisibility()));

    /* --- logo: some quando o carrossel assume, e fica escondido enquanto
       houver menu aberto.

       Antes ele voltava assim que a mao saia do alcance, mesmo com o menu
       ainda aberto - dava logo e titulo do menu na mesma tela. Sair do
       alcance nao fecha o menu (a pessoa pode estar so abaixando o braco),
       entao o logo tem de respeitar isso.                              */
    {
        uint8_t a = inMenu
                  ? 0
                  : (uint8_t)(255 * (1.0f - menu.getVisibility()));
        if (a != hero.getAlpha())
        {
            hero.setAlpha(a);
            hero.invalidate();
        }
    }

    /* --- icones do carrossel --- */
    touchgfx::Image* ic[5] = { &icon0, &icon1, &icon2, &icon3, &icon4 };
    for (int i = 0; i < 5; i++)
    {
        int16_t cx, cy, sz;
        uint8_t al;
        menu.getIconPose(i, cx, cy, sz, al);

        ic[i]->invalidate();
        /* guardo o id anterior em vez de perguntar ao widget: a assinatura
           de getBitmap() muda entre versoes do TouchGFX, e trocar o bitmap
           toda vez custaria uma releitura da flash por icone por frame.  */
        uint16_t id = ICON_ID[i][iconSlot(sz)];
        if (id != lastIconId[i])
        {
            lastIconId[i] = id;
            ic[i]->setBitmap(Bitmap(id));
        }
        ic[i]->setXY((int16_t)(cx - sz / 2), (int16_t)(cy - sz / 2));
        ic[i]->setAlpha(al);
        ic[i]->invalidate();
    }

    /* --- anel do item selecionado --- */
    {
        int st = menu.getDwellStage();
        int sel = menu.getSelected();
        int16_t cx, cy, sz;
        uint8_t al;
        menu.getIconPose(sel, cx, cy, sz, al);

        selRing.invalidate();
        if (al > 90 && !inMenu)
        {
            /* os dois lados explicitamente uint16_t: um enum e um inteiro
               no mesmo ternario geram aviso com -Wextra                  */
            uint16_t rid = (st >= 0)
                         ? (uint16_t)(BITMAP_RING_00_ID + st)
                         : (uint16_t)BITMAP_RING_IDLE_ID;
            selRing.setBitmap(Bitmap(rid));
            selRing.setXY((int16_t)(cx - 42), (int16_t)(cy - 42));
            selRing.setAlpha((uint8_t)(255 * menu.getVisibility()));
        }
        else
        {
            selRing.setAlpha(0);
        }
        selRing.invalidate();
    }

    /* --- rotulo e mensagem --- */
    {
        int sel = inMenu ? screen : menu.getSelected();
        uint8_t a = inMenu ? 255 : (uint8_t)(255 * menu.getVisibility());

        lblOption.invalidate();
        lblOption.setBitmap(Bitmap(LBL_ID[sel]));
        int16_t lx = inMenu ? LBL_MENU : LBL_CX;
        lblOption.setXY((int16_t)(lx - LBL_W[sel] / 2),
                        (int16_t)(LBL_CY - LBL_H[sel] / 2));
        lblOption.setAlpha(a);
        lblOption.invalidate();

        int m = inMenu ? 3 : (int)menu.getStateMsg();
        msgState.invalidate();
        if (m < 3)
        {
            msgState.setBitmap(Bitmap(MSG_ID[m]));
            msgState.setXY((int16_t)(MSG_CX - 8),
                           (int16_t)(LBL_CY - MSG_H[m] / 2));
            msgState.setAlpha((uint8_t)(170 * menu.getVisibility()));
        }
        else
        {
            msgState.setBitmap(Bitmap(BITMAP_MSG_BACK_ID));
            msgState.setXY((int16_t)(MSG_CX - 8),
                           (int16_t)(LBL_CY - MSG_BACK_H / 2));
            msgState.setAlpha(160);
        }
        msgState.invalidate();
    }

    /* --- textos da tela de espera: o inverso do carrossel ---
       Somem mais rapido do que o carrossel aparece (fator 1.6), para os
       dois conjuntos de texto nao conviverem na tela durante a
       transicao.                                                       */
    {
        float v = 1.0f - menu.getVisibility() * 1.6f;
        if (v < 0.0f) v = 0.0f;
        uint8_t a = inMenu ? 0 : (uint8_t)(255 * v);
        if (a != sbReady.getAlpha())
        {
            sbReady.invalidate();
            sbReady.setAlpha(a);
            sbWave.setAlpha((uint8_t)(a * 0.72f));
            sbReady.invalidate();
            sbWave.invalidate();
        }
    }

    /* Keep the divider visible; move it aside while interacting with the UI. */
    {
        const int16_t lineX = (handPresent || inMenu) ? 394 : 314;
        const int16_t sparkX = (handPresent || inMenu) ? 385 : 305;
        if (divLine.getX() != lineX || divSpark.getX() != sparkX)
        {
            divLine.invalidate();
            divSpark.invalidate();
            divLine.setXY(lineX, 109);
            divSpark.setX(sparkX);
            divLine.invalidate();
            divSpark.invalidate();
        }
    }

    /* --- borda: respira ao entrar ou sair de um menu --- */
    {
        int ev = menu.takeNavEvent();
        if (ev == 1)
        {
            presenter->pulseMenuEnterLED();
        }
        if (ev != 0)
        {
            /* entrar usa pulso mais forte que sair: com a mesma intensidade
               os dois eventos ficariam indistinguiveis                    */
            rimGlow.setAlpha((ev == 1) ? 255 : 200);
            rimGlow.invalidate();
        }
        else
        {
            uint8_t a = rimGlow.getAlpha();
            if (a > 140)
            {
                rimGlow.setAlpha((uint8_t)(a - ((a - 140) / 8 + 1)));
                rimGlow.invalidate();
            }
        }
    }

    lastScreen = screen;
}

void Screen1View::handleClickEvent(const touchgfx::ClickEvent& evt)
{
    bool down = (evt.getType() == touchgfx::ClickEvent::PRESSED);
    /* o display esta girado: o X da tela corresponde ao Y da cena */
    setHand(down,
            (240 - evt.getY()) / 240.0f,
            (evt.getX() - 240) / 240.0f);
}

void Screen1View::handleDragEvent(const touchgfx::DragEvent& evt)
{
    setHand(true,
            (240 - evt.getNewY()) / 240.0f,
            (evt.getNewX() - 240) / 240.0f);
}
