#ifndef AURA_CALENDAR_LOGIC_HPP
#define AURA_CALENDAR_LOGIC_HPP
#include "calendar_data.h"
#include <math.h>
/* One action per excursion; entry first requires a neutral/released hand. */
class CalendarLogic {
public:
    unsigned year=0, month=0;
    void enter(const CalSnapshot& s) { year=s.time_valid?s.now.year:0; month=s.time_valid?s.now.month:0; armed=false; direction=0; browsed=false; }
    void synchronize(const CalSnapshot& s) {
        if (s.time_valid && (!year || !browsed)) { year=s.now.year; month=s.now.month; }
    }
    int gesture(bool present,float x,float y) {
        if (!present || (fabsf(x)<0.28f && fabsf(y)<0.28f)) { armed=true; direction=0; return 0; }
        if (!armed) return 0;
        int next=0;
        if (y < -0.65f && -y>fabsf(x)) next=2;
        else if (fabsf(x)>=0.55f && fabsf(x)>=fabsf(y)) next=x>0?1:-1;
        if (!next || next==direction) return 0;
        direction=next;
        if (next==2) { armed=false; return 2; }
        if (!year) return 0;
        int serial=(int)year*12+(int)month-1+next;
        if (serial<CAL_FIRST_YEAR*12 || serial>CAL_LAST_YEAR*12+11) return 0;
        year=serial/12; month=serial%12+1; browsed=true;
        return next;
    }
private:
    bool armed=false, browsed=false;
    int direction=0;
};
#endif
