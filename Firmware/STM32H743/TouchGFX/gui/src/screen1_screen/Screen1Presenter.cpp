#include <gui/screen1_screen/Screen1View.hpp>
#include <gui/screen1_screen/Screen1Presenter.hpp>

Screen1Presenter::Screen1Presenter(Screen1View& v)
    : view(v)
{

}

void Screen1Presenter::activate()
{

}

void Screen1Presenter::deactivate()
{

}

void Screen1Presenter::handUpdated(bool present, float x, float y)
{
    view.setHand(present, x, y);
}

void Screen1Presenter::playMenuSound(MenuSound sound)
{
    model->playMenuSound(sound);
}

void Screen1Presenter::requestSetting(uint8_t item, int8_t delta)
{
    model->requestSetting(item, delta);
}

void Screen1Presenter::handClicked()
{
    view.requestHandClick();
}

void Screen1Presenter::wifiLevelUpdated(uint8_t level)
{
    view.setWifiLevel(level);
}

void Screen1Presenter::settingTextUpdated(uint8_t item, const char* text)
{
    view.setSettingText(item, text);
}

void Screen1Presenter::volumeUpdated(uint8_t volume)
{
    view.setVolume(volume);
}

void Screen1Presenter::setHeroBreath(uint8_t level)
{
    model->setHeroBreath(level);
}

void Screen1Presenter::setCarouselLED(float angle, uint8_t visibility)
{
    model->setCarouselLED(angle, visibility);
}

void Screen1Presenter::pulseMenuEnterLED()
{
    model->pulseMenuEnterLED();
}

bool Screen1Presenter::startupReady() const
{
    return model->startupReady();
}

void Screen1Presenter::completeStartup()
{
    model->completeStartup();
}
