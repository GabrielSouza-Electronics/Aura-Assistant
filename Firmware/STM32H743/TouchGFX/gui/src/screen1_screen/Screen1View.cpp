#include "TypographyHints.hpp"
#include "StartupAssets.hpp"
#include <math.h>
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
/* Center labels using the installed bitmap dimensions after rotation. */

static const uint16_t MSG_ID[3] = {
    BITMAP_MSG_HOLD_ID, BITMAP_MSG_CANCELLED_ID, BITMAP_MSG_MOVE_ID
};

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

/* --- menu Settings ------------------------------------------------------
   Retangulos e colunas vem de SettingsLayout.hpp (gerado pelo
   gen_settings.py). IDs em tabela explicita, sem aritmetica: a pasta
   settings/ mistura sequencias (glow, row, rowf, val).                  */
static const int SCREEN_SETTINGS = 0;     /* indice do item no carrossel  */

static const uint16_t SET_ROW_ID[SL_ROWS] = {
    BITMAP_ROW_0_ID, BITMAP_ROW_1_ID, BITMAP_ROW_2_ID, BITMAP_ROW_3_ID
};
static const uint16_t SET_ROWF_ID[SL_ROWS] = {
    BITMAP_ROWF_0_ID, BITMAP_ROWF_1_ID, BITMAP_ROWF_2_ID, BITMAP_ROWF_3_ID
};
static const uint16_t SET_GLOW_ID[SL_ROWS] = {
    BITMAP_GLOW_0_ID, BITMAP_GLOW_1_ID, BITMAP_GLOW_2_ID, BITMAP_GLOW_3_ID
};

static const int SET_ROW_SOUND = 3;       /* linha que mostra o volume    */
static const uint8_t VOLUME_MAX = 10;

/* Textos de demonstracao do simulador. No alvo o Model substitui pelos
   valores do App (APP_UISettings_SetText) no primeiro tick.          */
static const char* const SET_DEMO_TEXT[SET_ROW_SOUND] = {
    "HomeNet", "On", "70%"
};

/* Fixed 14 px gesture hints; the dynamic value atlas uses 18 px glyphs. */
static const char* const HINT_LIST = "RIGHT SELECT  LEFT BACK";
static const char* const HINT_EDIT = "UP/DOWN CHANGE  LEFT DONE";

static int16_t roundi(float v)
{
    return (int16_t)((v >= 0.0f) ? (v + 0.5f) : (v - 0.5f));
}

/* Parametros das particulas: brilho reduzido em qualquer submenu. */
static const float FIELD_LEVEL_MENU = 0.5f;
static const float FIELD_LEVEL_K    = 0.12f;

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
    for (int i = 0; i < SL_ROWS; i++)
    {
        lastRowId[i] = 0;
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
    sbReady.setXY((int16_t)(292 - sbReady.getWidth() / 2),
                  (int16_t)(239 - sbReady.getHeight() / 2));
    sbWave.setBitmap(Bitmap(BITMAP_SB_WAVE_ID));
    sbWave.setXY((int16_t)(348 - sbWave.getWidth() / 2),
                 (int16_t)(239 - sbWave.getHeight() / 2));

    setupSettings();
    // The existing perimeter rim remains above the calendar content.
    insert(&battFrame, calendar);

    applyStatus();
    heroHomeX = hero.getX();
    heroHomeY = hero.getY();
    startupStage = StartupStage::Rise;
    startupTicks = 0;
    setHomeVisible(false);
    startupAnimationTicks = 0;
    const int16_t textY[3] = {347, 388, 409};
    for (int i = 0; i < 3; ++i)
    {
        const StartupSprite& sprite = startupSprites[i == 0 ? 0 : i + 3];
        startupText[i].setBitmapFormat(Bitmap::ARGB8888);
        startupText[i].setPixelData(reinterpret_cast<uint8_t*>(const_cast<uint32_t*>(sprite.pixels)));
        startupText[i].setPosition(textY[i] - sprite.width / 2,
                                  (480 - sprite.height) / 2, sprite.width, sprite.height);
        add(startupText[i]);
    }
    divLine.setXY(370 - divLine.getWidth() / 2, (480 - divLine.getHeight()) / 2);
    divSpark.setXY(370 - divSpark.getWidth() / 2, (480 - divSpark.getHeight()) / 2);
    divLine.setAlpha(255);
    divSpark.setAlpha(255);
    divLine.setVisible(true);
    divSpark.setVisible(true);
    // Assets are rotated: physical bottom is framebuffer +X.
    hero.setXY(480, (480 - hero.getHeight()) / 2);
    hero.setAlpha(255);
}

void Screen1View::setupSettings()
{
    /* Pilha: field (particulas) < hero < halo < caixas < valores <
       subtitulo < icones/titulo. insert() encadeado mantem essa ordem
       sem mexer no Designer.                                          */
    Drawable* prev = &hero;
    setGlow.setBitmap(Bitmap(SET_GLOW_ID[0]));
    insert(prev, setGlow);
    prev = &setGlow;
    for (int i = 0; i < SL_ROWS; i++)
    {
        setRow[i].setBitmap(Bitmap(SET_ROW_ID[i]));
        lastRowId[i] = SET_ROW_ID[i];
        insert(prev, setRow[i]);
        prev = &setRow[i];
    }
    for (int i = 0; i < SL_ROWS; i++)
    {
        insert(prev, setVal[i]);
        prev = &setVal[i];
    }
    for (int i = 0; i < SET_ROW_SOUND; i++)
    {
        setSettingText((uint8_t)i, SET_DEMO_TEXT[i]);
    }
    setVolume(5);
    setSub.setBitmap(Bitmap(BITMAP_SUB_TITLE_ID));
    setSub.setXY((int16_t)(SL_SUB_CX_FB - SL_SUB_W / 2),
                 (int16_t)(239 - SL_SUB_H / 2));
    insert(prev, setSub);
    /* a dica fica no fim da pilha, junto do msgState que ela substitui */
    add(setHint);

    Drawable* all[] = { &setGlow, &setSub, &setHint };
    for (Drawable* d : all) d->setVisible(false);
    for (int i = 0; i < SL_ROWS; i++)
    {
        setRow[i].setVisible(false);
        setVal[i].setVisible(false);
    }
    settingsShown = false;
}

void Screen1View::applySettings()
{
    if (!settings.isVisible() && !settingsShown) return;
    settingsShown = settings.isVisible();

    const int focus = settings.getFocus();
    /* Offsets LOGICOS da linha em foco (inclinacao, batida, ajuste).
       Girado: x logico -> -y do framebuffer, y logico -> +x.          */
    const int16_t fdx = roundi(settings.getFocusOffsetY());
    const int16_t fdy = roundi(-settings.getFocusOffsetX());
    for (int i = 0; i < SL_ROWS; i++)
    {
        const uint8_t a = settings.getRowAlpha(i);
        const int16_t dx = (i == focus) ? fdx : 0;
        const int16_t dy = (int16_t)(-settings.getRowSlide(i)
                                     + ((i == focus) ? fdy : 0));
        const SettingsRect& r = SL_ROW[i];

        setRow[i].invalidate();
        const uint16_t id = (i == focus) ? SET_ROWF_ID[i] : SET_ROW_ID[i];
        if (id != lastRowId[i])
        {
            lastRowId[i] = id;
            setRow[i].setBitmap(Bitmap(id));
        }
        setRow[i].setXY((int16_t)(r.x + dx), (int16_t)(r.y + dy));
        setRow[i].setAlpha(a);
        setRow[i].setVisible(a > 0);
        setRow[i].invalidate();

        /* valor alinhado a direita no logico = topo fixo no fb */
        setVal[i].invalidate();
        setVal[i].setXY((int16_t)(r.x + dx + r.w / 2 - setVal[i].getWidth() / 2),
                        (int16_t)(SL_VALUE_END_FB_Y + dy));
        setVal[i].setAlpha(settings.getValueAlpha(i));
        setVal[i].setVisible(a > 0);
        setVal[i].invalidate();
    }

    setGlow.invalidate();
    if (focus >= 0)
    {
        /* o halo acompanha a posicao continua da lista (como o angulo
           do carrossel); as linhas tem o mesmo passo, posicao linear  */
        const SettingsRect& g = SL_GLOW[focus];
        const float row = settings.getGlowRow();
        setGlow.setBitmap(Bitmap(SET_GLOW_ID[focus]));
        setGlow.setXY((int16_t)(SL_GLOW[0].x + roundi(row * SL_ROW_PITCH) + fdx),
                      (int16_t)(g.y - settings.getRowSlide(focus) + fdy));
        const uint8_t a = (uint8_t)((settings.getGlowAlpha()
                                     * settings.getRowAlpha(focus)) / 255);
        setGlow.setAlpha(a);
        setGlow.setVisible(a > 0);
    }
    else
    {
        setGlow.setVisible(false);
    }
    setGlow.invalidate();

    setSub.invalidate();
    const uint8_t sa = (uint8_t)((settings.getRowAlpha(0) * 200) / 255);
    setSub.setAlpha(sa);
    setSub.setVisible(sa > 0);
    setSub.invalidate();

    /* dica de gesto conforme o estado; so reescreve quando muda */
    const char* hint = settings.isEditing() ? HINT_EDIT : HINT_LIST;
    setHint.invalidate();
    if (hint != hintText)
    {
        hintText = hint;
        const TypographyHint& sprite = typographyHints[settings.isEditing() ? 1 : 0];
        setHint.setBitmapFormat(Bitmap::ARGB8888);
        setHint.setPixelData(reinterpret_cast<uint8_t*>(const_cast<uint32_t*>(sprite.pixels)));
        setHint.setWidth(sprite.width);
        setHint.setHeight(sprite.height);
    }
    setHint.setXY(MSG_CX - setHint.getWidth() / 2, (int16_t)((480 - setHint.getHeight()) / 2));
    const uint8_t ha = (uint8_t)((settings.getRowAlpha(SL_ROWS - 1) * 170) / 255);
    setHint.setAlpha(ha);
    setHint.setVisible(ha > 0);
    setHint.invalidate();
}

void Screen1View::handleSettingsEvents()
{
    int8_t item = 0;
    int8_t delta = 0;
    if (settings.takeRequest(item, delta))
    {
        presenter->requestSetting((uint8_t)item, delta);
    }
    switch (settings.takeFeedback())
    {
    case SettingsLogic::Feedback::Move:
    case SettingsLogic::Feedback::Change:
        presenter->playMenuSound(MenuSound::Tick);
        break;
    case SettingsLogic::Feedback::Enter:
        presenter->playMenuSound(MenuSound::Enter);
        presenter->pulseMenuEnterLED();
        break;
    case SettingsLogic::Feedback::Leave:
        presenter->playMenuSound(MenuSound::Exit);
        break;
    default:
        break;      /* Bump: so visual */
    }
}

void Screen1View::setSettingText(uint8_t item, const char* text)
{
    if (item >= SET_ROW_SOUND) return;
    setVal[item].invalidate();
    setVal[item].setText(text, SL_VALUE_MAX_W[item]);
    setVal[item].invalidate();
}

void Screen1View::setVolume(uint8_t volume)
{
    if (volume > VOLUME_MAX) volume = VOLUME_MAX;
    char text[6];
    int n = 0;
    if (volume >= 10) text[n++] = (char)('0' + volume / 10);
    text[n++] = (char)('0' + volume % 10);
    text[n++] = '/';
    text[n++] = '1';
    text[n++] = '0';
    text[n] = '\0';
    setVal[SET_ROW_SOUND].invalidate();
    setVal[SET_ROW_SOUND].setText(text, SL_VALUE_MAX_W[SET_ROW_SOUND]);
    setVal[SET_ROW_SOUND].invalidate();
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
    // Independent wrapping phase keeps animating during an indefinite sensor wait.
    startupAnimationTicks = (startupAnimationTicks + 1) % 360;
    const uint8_t alpha = startupStage == StartupStage::Dock
        ? (uint8_t)(255U * (36U - startupTicks) / 36U) : 255;
    if (startupAnimationTicks % 45 == 0)
    {
        startupText[0].invalidate();
        startupText[0].setPixelData(reinterpret_cast<uint8_t*>(const_cast<uint32_t*>(
            startupSprites[(startupAnimationTicks / 45) % 4].pixels)));
        startupText[0].invalidate();
    }
    for (int i = 0; i < 3; ++i)
    {
        if (startupText[i].getAlpha() != alpha)
        {
            startupText[i].setAlpha(alpha);
            startupText[i].invalidate();
        }
    }
    divSpark.invalidate();
    const int16_t lightX = (int16_t)(240 + 110 * sinf(startupAnimationTicks * 6.2831853f / 360));
    divSpark.setY(480 - lightX - divSpark.getHeight() / 2);
    divSpark.setAlpha(alpha);
    divSpark.invalidate();
    if (divLine.getAlpha() != alpha) { divLine.setAlpha(alpha); divLine.invalidate(); }
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
            for (int i = 0; i < 3; ++i) startupText[i].setVisible(false);
            divLine.setXY(314, 109);
            divSpark.setXY(305, 226);
            divLine.setAlpha(190);
            divSpark.setAlpha(190);
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
    const bool wasSettings = (previousScreen == SCREEN_SETTINGS);
    const bool wasCalendar = (previousScreen == 3);
    const bool click = handClickPending;
    handClickPending = false;

    /* Dentro do Settings os gestos sao dele: o eixo vertical move o foco
       (sem "puxar para baixo = voltar") e o clique confirma a linha.  */
    menu.setVerticalBack(!wasSettings && !wasCalendar);
    menu.tick(handPresent, handX, handY, (wasSettings || wasCalendar) ? false : click);
    if (menu.getScreen() == 3)
    {
        if (!wasCalendar) calendar.enter();
        else if (calendar.tick(handPresent, handX, handY)) menu.close();
        // Month change uses the same feedback as moving between options.
        if (calendar.takeMonthChanged()) presenter->playMenuSound(MenuSound::Tick);
    }
    if (calendar.isVisible() && menu.getScreen() != 3)
    {
        calendar.setVisible(false);
        invalidate();
    }
    const bool openNow = (menu.getScreen() == SCREEN_SETTINGS);
    settings.tick(openNow, handPresent, handX, handY,
                  openNow && wasSettings && click);
    if (settings.takeExit())
    {
        menu.close();
    }
    handleSettingsEvents();
    const bool inSettings = (menu.getScreen() == SCREEN_SETTINGS);
    /* LED breath hue: Settings uses the emerald focus colour of its rows
       (gen_settings.py C_FOCUS 40,220,150, scaled to full brightness). */
    if (inSettings) presenter->setLEDBreathColor(46, 255, 174);
    else presenter->setLEDBreathColor(0, 255, 255);

    const MenuSound sound = menuSoundForTransition(previousScreen, previousItem,
        menu.getScreen(), menu.getSelected(), previousHandPresent, handPresent);
    previousHandPresent = handPresent;
    if (sound != MenuSound::None)
        presenter->playMenuSound(sound);
    applySettings();
    updateRim();

    const bool inCalendar = (menu.getScreen() == 3);
    if (inCalendar != wasCalendar)
    {
        Drawable* home[] = { &hero, &icon0, &icon1, &icon2,
            &icon3, &icon4, &selRing, &lblOption, &divLine, &divSpark, &msgState,
            &sbReady, &sbWave };
        // Keep the shared live status header visible while viewing the calendar.
        for (Drawable* widget : home) widget->setVisible(!inCalendar);
        board.setAlpha(inCalendar ? 102 : 255); // 40%, independent of backlight.
        /* The hidden Hero keeps animating: its frames drive the LED breath. */
        if (inCalendar) divSpark.stopAnimation();
        else divSpark.startAnimation(false, false, true);
        invalidate();
    }
    if (inCalendar)
    {
        fieldLevel = 0.4f;
        field.setMasterAlpha(102);
        // Continue the existing particles along circuit routes behind the text.
        field.tick(0.0f, -menu.getAngle() * 0.45f * 57.2958f);
        presenter->setCarouselLED(menu.getAngle(), 0U);
        lastScreen = 3;
        return;
    }

    /* a esfera acompanha o carrossel com fator MENOR. Girando junto, ela e
       os icones formariam um bloco rigido; a diferenca de velocidade e o
       que da a leitura de duas camadas.                                  */
    field.tick(menu.getVisibility(), -menu.getAngle() * 0.45f * 57.2958f);

    const int screen = menu.getScreen();
    const bool inMenu = (screen >= 0);

    /* particulas a meio brilho dentro de qualquer submenu, brilho cheio
       no carrossel e na tela inicial; transicao suave nos dois sentidos */
    {
        const float target = inMenu ? FIELD_LEVEL_MENU : 1.0f;
        fieldLevel += (target - fieldLevel) * FIELD_LEVEL_K;
        field.setMasterAlpha((uint8_t)(255.0f * fieldLevel + 0.5f));
    }
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
        if (inSettings)
        {
            lx = (int16_t)(LBL_MENU + (SL_TITLE_CX_FB - LBL_MENU)
                                      * settings.getHeaderProgress());
        }
        lblOption.setXY((int16_t)(lx - lblOption.getWidth() / 2),
                        (int16_t)(LBL_CY - lblOption.getHeight() / 2));
        lblOption.setAlpha(a);
        lblOption.invalidate();

        int m = inMenu ? 3 : (int)menu.getStateMsg();
        msgState.invalidate();
        if (m < 3)
        {
            msgState.setBitmap(Bitmap(MSG_ID[m]));
            msgState.setXY((int16_t)(MSG_CX - msgState.getWidth() / 2),
                           (int16_t)(LBL_CY - msgState.getHeight() / 2));
            msgState.setAlpha((uint8_t)(170 * menu.getVisibility()));
        }
        else
        {
            msgState.setBitmap(Bitmap(BITMAP_MSG_BACK_ID));
            msgState.setXY((int16_t)(MSG_CX - msgState.getWidth() / 2),
                           (int16_t)(LBL_CY - msgState.getHeight() / 2));
            /* no Settings a dica de gesto (setHint) ocupa este lugar */
            msgState.setAlpha(inSettings ? 0 : 160);
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

    lastScreen = screen;
}

void Screen1View::updateRim()
{
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

}

void Screen1View::handleClickEvent(const touchgfx::ClickEvent& evt)
{
    bool down = (evt.getType() == touchgfx::ClickEvent::PRESSED);
    /* o display esta girado: o X da tela corresponde ao Y da cena.
       Mesma convencao do ToF (APP_HandTracking_ReadPointer): positivo =
       direita / CIMA. X do framebuffer cresce para BAIXO fisico, dai o
       sinal invertido no segundo eixo.                                 */
    setHand(down,
            (240 - evt.getY()) / 240.0f,
            (240 - evt.getX()) / 240.0f);
}

void Screen1View::handleDragEvent(const touchgfx::DragEvent& evt)
{
    setHand(true,
            (240 - evt.getNewY()) / 240.0f,
            (240 - evt.getNewX()) / 240.0f);
}
