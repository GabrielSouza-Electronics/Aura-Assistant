#pragma once
#include <math.h>
#include <gui/common/HandInput.hpp>
class TasksLogic {
public:
    unsigned page=0, row=0, ticks=0;
    int arrow=0;
    void enter() { page=row=ticks=hold=0; armed=false; direction=arrow=0; progress=0; }
    float headerProgress() const { float t=fminf(ticks/14.0f,1.0f); return t*t*(3-2*t); }
    // -1 back; 1 changed selection/page; 0 no action.
    int tick(bool present,float x,float y,unsigned total) {
        if (ticks<14) ++ticks;
        unsigned pages=(total+3)/4;
        if (!pages) page=row=0;
        else if (page>=pages) page=pages-1;
        unsigned count=total>page*4?((total-page*4)>4?4:total-page*4):0;
        if (!count) row=0; else if (row>=count) row=count-1;
        if (!present || (fabsf(x)<=HandInput::CENTER_ENTER && fabsf(y)<=HandInput::CENTER_ENTER)) {
            armed=true; direction=arrow=0; progress=0; hold=0; return 0;
        }
        if (!armed) return 0;
        // Menu exit is handled globally by the timed ToF proximity gesture.
        hold=0;
        int next=0;
        float magnitude=0;
        if (fabsf(x)>=fabsf(y) && fabsf(x)>=HandInput::CENTER_EXIT) {
            next=x>0?1:-1; magnitude=fabsf(x);
            arrow=(next>0?page+1<pages:page>0)?next:0;
        } else if (fabsf(y)>=HandInput::CENTER_EXIT) {
            next=y<0?2:-2; magnitude=fabsf(y); arrow=0;
        } else { direction=arrow=0; progress=0; return 0; }
        if (direction!=next) { direction=next; progress=0; }
        else { progress+=fminf(magnitude,1.0f); if (progress<30) return 0; progress-=30; }
        if (next==1 && page+1<pages) { ++page; row=0; return 1; }
        if (next==-1 && page>0) { --page; row=0; return 1; }
        if (next==2 && row+1<count) { ++row; return 1; }
        if (next==-2 && row>0) { --row; return 1; }
        arrow=0; return 0;
    }
private:
    unsigned hold=0;
    bool armed=false;
    int direction=0;
    float progress=0;
};
