#ifndef SCREEN1VIEW_HPP
#define SCREEN1VIEW_HPP

#include <gui_generated/screen1_screen/Screen1ViewBase.hpp>
#include <gui/screen1_screen/Screen1Presenter.hpp>

#include <gui/common/CircuitField.hpp>
#include <gui/common/MenuLogic.hpp>
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
    void setBatteryLevel(int pct);
    void setChargerStatus(bool chg);

protected:
    CircuitField field;
    MenuLogic    menu;

    bool  handPresent;
    bool  handClickPending = false;
    bool  previousHandPresent;
    float handX, handY;
    int   wifiLevel;
    int   battLevel;
    bool  charging;
    int   lastScreen;
    uint16_t lastIconId[5];      /* evita setBitmap redundante             */

    void applyStatus();
    enum class StartupStage { Rise, Wait, Dock, Done };
    StartupStage startupStage = StartupStage::Rise;
    uint16_t startupTicks = 0;
    int16_t heroHomeX = 0;
    int16_t heroHomeY = 0;
    void setHomeVisible(bool visible);
    bool tickStartup();
};



#endif // SCREEN1VIEW_HPP
