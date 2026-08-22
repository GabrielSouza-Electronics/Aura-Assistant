#ifndef INITIALPRESENTER_HPP
#define INITIALPRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

using namespace touchgfx;

class InitialView;

class InitialPresenter : public touchgfx::Presenter, public ModelListener
{
public:
    InitialPresenter(InitialView& v);

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

    virtual ~InitialPresenter() {}

private:
    InitialPresenter();

    InitialView& view;
};

#endif // INITIALPRESENTER_HPP
