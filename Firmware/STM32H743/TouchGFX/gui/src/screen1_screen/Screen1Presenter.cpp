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
