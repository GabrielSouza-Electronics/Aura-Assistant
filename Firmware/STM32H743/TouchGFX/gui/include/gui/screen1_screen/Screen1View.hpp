#ifndef SCREEN1VIEW_HPP
#define SCREEN1VIEW_HPP

#include <gui_generated/screen1_screen/Screen1ViewBase.hpp>
#include <gui/screen1_screen/Screen1Presenter.hpp>

#include <gui/common/CircuitField.hpp>
#include <gui/common/MenuLogic.hpp>
#include <gui/common/AvatarAnimation.hpp>
#include <gui/common/SettingsLogic.hpp>
#include <gui/common/GlyphText.hpp>
#include <gui/common/CalendarWidget.hpp>
#include <gui/common/TasksWidget.hpp>
#include <touchgfx/widgets/Image.hpp>
#include <touchgfx/widgets/PixelDataWidget.hpp>
#include <touchgfx/events/ClickEvent.hpp>
#include <touchgfx/events/DragEvent.hpp>

class Screen1View : public Screen1ViewBase
{
public:
    Screen1View();
    virtual ~Screen1View() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
    virtual void handleTickEvent();

    /* teste no simulador sem sensores:
       segurar = mao presente, arrastar = mover a mao                      */
    virtual void handleClickEvent(const touchgfx::ClickEvent& evt);
    virtual void handleDragEvent(const touchgfx::DragEvent& evt);

    /* --- entradas do sistema ------------------------------------------- */
    void setHand(bool present, float x, float y)
    {
        handPresent = present;
        handX = x;                  /* -1 .. +1 */
        handY = y;
    }
    void setWifiLevel(int lvl);
    void requestHandClick() { handClickPending = true; }
    void setHandNear(bool closeRange) { handNear = closeRange; }
    void requestHandBack() { handBackPending = true; }
    void setBatteryLevel(int pct);
    void setChargerStatus(bool chg);
    /* Settings: textos livres (0 Wi-Fi, 1 Bluetooth, 2 Brightness) e
       volume 0..10 exibido como "N/10" na linha Sound.                 */
    void setSettingText(uint8_t item, const char* text);
    void setVolume(uint8_t volume);
    void setAvatarFlags(bool preparing, bool speaking) { avatarPreparing=preparing; avatarSpeaking=speaking; }

protected:
    CircuitField field;
    MenuLogic    menu;
    CalendarWidget calendar;
    TasksWidget tasks;

    /* Menu Settings: widgets criados em codigo, como o field. Ficam sobre
       as particulas e sob o titulo; as caixas sao translucidas.         */
    SettingsLogic   settings;
    touchgfx::Image setGlow;
    touchgfx::Image setFocusFrame;
    touchgfx::Image setRow[SL_ROWS];
    GlyphText       setVal[SL_ROWS];
    touchgfx::Image       setHint;        /* dica de gesto, no lugar do msgState */
    const char*     hintText = nullptr;
    touchgfx::Image setSub;
    uint16_t lastRowId[SL_ROWS];
    bool     settingsShown = false;
    float    fieldLevel = 1.0f;     /* brilho das particulas, 0.5 em submenu */

    bool  handPresent;
    bool  handClickPending = false;
    bool  handNear = false;
    bool  handBackPending = false;
    unsigned simulatorNearTicks = 0;
    bool  previousHandPresent;
    float handX, handY;
    int   wifiLevel;
    int   battLevel;
    bool  charging;
    int   lastScreen;
    unsigned chatHeaderTicks = 0;
    touchgfx::Image avatar;
    touchgfx::Image chatRay[6];
    touchgfx::Image chatRaySpark;
    AvatarAnimation avatarAnimation;
    bool avatarPreparing=false, avatarSpeaking=false;
    uint16_t lastIconId[5];      /* evita setBitmap redundante             */

    void applyStatus();
    void updateRim();
    void setupSettings();
    void applySettings();
    void handleSettingsEvents();
    enum class StartupStage { Rise, Wait, Dock, Done };
    StartupStage startupStage = StartupStage::Rise;
    uint16_t startupTicks = 0;
    uint16_t startupAnimationTicks = 0;
    uint16_t startupSparkTicks = 0;     /* spark slide phase, starts with the line */
    touchgfx::Image startupText[3];
    int16_t heroHomeX = 0;
    int16_t heroHomeY = 0;
    void setHomeVisible(bool visible);
    bool tickStartup();
};



#endif // SCREEN1VIEW_HPP
