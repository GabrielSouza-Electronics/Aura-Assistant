#ifndef AURA_CALENDAR_LOGIC_HPP
#define AURA_CALENDAR_LOGIC_HPP
#include "calendar_data.h"
#include <gui/common/HandInput.hpp>
#include <math.h>
/* Held horizontal input repeats; entry requires a neutral/released hand.
   Back is handled globally by a ToF <30 mm hold for two seconds.
   Like MenuLogic, speed is proportional to displacement and tick-based. */
class CalendarLogic {
public:
    unsigned year=0, month=0;
    void enter(const CalSnapshot& s) { year=s.time_valid?s.now.year:0; month=s.time_valid?s.now.month:0; armed=false; direction=0; progress=0; backHold=0; browsed=false; }
    int getDirection() const { return direction; }
    void synchronize(const CalSnapshot& s) {
        if (s.time_valid && (!year || !browsed)) { year=s.now.year; month=s.now.month; }
    }
    int gesture(bool present,float x,float y) {
        if (!present || (fabsf(x)<=HandInput::CENTER_ENTER && fabsf(y)<=HandInput::CENTER_ENTER)) {
            armed=true; direction=0; progress=0; backHold=0; return 0;
        }
        if (!armed) return 0;
        int next=0;
        if (fabsf(x)>HandInput::CENTER_ENTER && fabsf(x)>=fabsf(y) &&
                 (fabsf(x)>=HandInput::CENTER_EXIT || direction==(x>0?1:-1))) next=x>0?1:-1;
        if (next!=2) backHold=0;
        if (!next) { direction=0; progress=0; return 0; }
        if (!year) { direction=0; progress=0; return 0; }
        const bool changed=next!=direction;
        direction=next;
        if (changed) progress=0;
        else {
            // Full deflection: one month per 40 ticks (~0.67 s at 60 Hz).
            progress+=fminf(fabsf(x),1.0f);
            if (progress<40.0f) return 0;
            progress-=40.0f;
        }
        int serial=(int)year*12+(int)month-1+next;
        if (serial<CAL_FIRST_YEAR*12 || serial>CAL_LAST_YEAR*12+11) {
            direction=0; progress=0; return 0;
        }
        year=serial/12; month=serial%12+1; browsed=true;
        return next;
    }
private:
    bool armed=false, browsed=false;
    int direction=0;
    float progress=0;
    int backHold=0;
};
#endif
