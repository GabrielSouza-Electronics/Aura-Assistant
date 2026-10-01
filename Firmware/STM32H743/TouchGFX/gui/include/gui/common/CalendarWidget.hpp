#ifndef AURA_CALENDAR_WIDGET_HPP
#define AURA_CALENDAR_WIDGET_HPP
#include <touchgfx/widgets/Widget.hpp>
#include <gui/common/CalendarLogic.hpp>
class CalendarWidget : public touchgfx::Widget {
public:
    CalendarWidget();
    void enter();
    bool tick(bool present,float x,float y);
    // True once after a hand gesture moved to another month.
    bool takeMonthChanged() { const bool c=monthChanged; monthChanged=false; return c; }
    virtual void draw(const touchgfx::Rect& area) const;
    // Text/markers overlay the existing animated board; no opaque background.
    virtual touchgfx::Rect getSolidRect() const { return touchgfx::Rect(); }
private:
    CalSnapshot data;
    CalendarLogic navigation;
    uint16_t ticks;
    uint8_t pulse;
    bool monthChanged=false;
    int status() const;
    void sprite(int id,int x,int y,const touchgfx::Rect& area,uint8_t alpha=255) const;
    void todayArea();
    void read();
};
#endif
