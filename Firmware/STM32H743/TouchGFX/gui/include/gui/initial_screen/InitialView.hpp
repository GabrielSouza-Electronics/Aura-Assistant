#ifndef INITIALVIEW_HPP
#define INITIALVIEW_HPP

#include <gui_generated/initial_screen/InitialViewBase.hpp>
#include <gui/initial_screen/InitialPresenter.hpp>

class InitialView : public InitialViewBase
{
public:
    InitialView();
    virtual ~InitialView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
};

#endif // INITIALVIEW_HPP
