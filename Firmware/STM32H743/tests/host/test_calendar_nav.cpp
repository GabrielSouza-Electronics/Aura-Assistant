#include <gui/common/CalendarLogic.hpp>
#include <assert.h>
int main()
{
    CalendarLogic nav;
    CalSnapshot s={}; s.time_valid=true; s.now.year=2026; s.now.month=12;
    nav.enter(s);
    assert(nav.year==2026 && nav.month==12);
    assert(nav.gesture(true,0.9f,0)==0); // Entry gesture is ignored.
    nav.gesture(false,0,0);
    assert(nav.gesture(true,0.9f,0)==1);
    assert(nav.year==2027 && nav.month==1);
    assert(nav.getDirection()==1);
    for (int i=0;i<44;i++) assert(nav.gesture(true,0.9f,0)==0);
    assert(nav.gesture(true,0.9f,0)==1);
    assert(nav.month==2);
    assert(nav.gesture(true,-0.9f,0)==-1); // Reversal acts immediately.
    assert(nav.month==1 && nav.getDirection()==-1);
    for (int i=0;i<44;i++) assert(nav.gesture(true,-0.9f,0)==0);
    assert(nav.gesture(true,-0.9f,0)==-1);
    assert(nav.year==2026 && nav.month==12);
    for (int i=0;i<100;i++) assert(nav.gesture(true,0.1f,0)==0);
    assert(nav.month==12 && nav.getDirection()==0);
    assert(nav.gesture(true,0.3f,0)==0); // Neutral hysteresis: no restart yet.
    assert(nav.gesture(true,0.5f,0)==1);
    for (int i=0;i<79;i++) assert(nav.gesture(true,0.5f,0)==0);
    assert(nav.gesture(true,0.5f,0)==1); // Half deflection repeats slower.
    for (int i=0;i<100;i++) assert(nav.gesture(false,1,0)==0);
    assert(nav.month==2 && nav.getDirection()==0);
    assert(nav.gesture(true,-1,0)==-1);
    assert(nav.gesture(true,0.3f,0)==0); // Reversal below start threshold stops.
    assert(nav.getDirection()==0);
    assert(nav.gesture(true,1,0)==1);
    assert(nav.gesture(true,0.8f,0.9f)==0); // Vertical input cancels repeat.
    assert(nav.getDirection()==0);
    for (int i=1;i<HandInput::BACK_HOLD;i++) assert(nav.gesture(true,0,-0.9f)==0);
    assert(nav.gesture(true,0,-0.9f)==2); // Back only after a 500 ms hold.
    nav.enter(s); assert(nav.month==12);
    s.now.month=1; s.now.year=2027; nav.synchronize(s);
    assert(nav.month==1 && nav.year==2027); // Midnight while following today.
    nav.gesture(false,0,0); nav.gesture(true,-0.8f,0);
    nav.synchronize(s); assert(nav.month==12 && nav.year==2026); // Preserve browsing.
    s.now.year=CAL_FIRST_YEAR; s.now.month=1; nav.enter(s);
    nav.gesture(false,0,0); assert(nav.gesture(true,-1,0)==0);
    for (int i=0;i<100;i++) assert(nav.gesture(true,-1,0)==0);
    assert(nav.getDirection()==0);
    s.now.year=CAL_LAST_YEAR; s.now.month=12; nav.enter(s);
    nav.gesture(false,0,0); assert(nav.gesture(true,1,0)==0);
    for (int i=0;i<100;i++) assert(nav.gesture(true,1,0)==0);
    assert(nav.getDirection()==0);
    s.time_valid=false; nav.enter(s); nav.gesture(false,0,0);
    assert(nav.gesture(true,1,0)==0 && nav.year==0);
    for (int i=1;i<HandInput::BACK_HOLD;i++) assert(nav.gesture(true,0,-1)==0);
    assert(nav.gesture(true,0,-1)==2); // Back also works before network sync.
}
