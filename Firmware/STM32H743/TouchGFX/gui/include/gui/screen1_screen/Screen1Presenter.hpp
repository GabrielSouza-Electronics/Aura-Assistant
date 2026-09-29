#ifndef SCREEN1PRESENTER_HPP
#define SCREEN1PRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

using namespace touchgfx;

class Screen1View;

class Screen1Presenter : public touchgfx::Presenter, public ModelListener
{
public:
    Screen1Presenter(Screen1View& v);

    /**
     * The activate function is called automatically when this screen is "switched in"
     * (ie. made active). Initialization logic can be placed here.
     */
    virtual void activate();

    /**
     * The deactivate function is called automatically when this screen is "switched out"
     * (ie. made inactive). Teardown functionality can be placed here.
     */
    virtual void deactivate();
    virtual void handUpdated(bool present, float x, float y);
    virtual void handClicked() override;
    void playMenuSound(MenuSound sound);
    void setHeroBreath(uint8_t level);
    void setCarouselLED(float angle, uint8_t visibility);
    void pulseMenuEnterLED();
    bool startupReady() const;
    void completeStartup();

    virtual ~Screen1Presenter() {}

private:
    Screen1Presenter();

    Screen1View& view;
};

#endif // SCREEN1PRESENTER_HPP
