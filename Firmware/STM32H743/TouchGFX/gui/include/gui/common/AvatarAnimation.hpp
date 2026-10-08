#pragma once
#include <stdint.h>

class AvatarAnimation
{
public:
    enum Clip { Idle, Blink, LookUp, LookDown, LookRight, LookLeft, Thinking, Goodbye, Coffee, Smile, Count };
    void enter(uint32_t now) { phase=Fade; clip=Idle; step=0; last=fadeStart=now; coffeeDue=now+60000; closing=done=false; prepIndex=0; rng=now^0x6d2b79f5U; }
    void requestClose() { closing=true; }
    bool closed() const { return done; }
    Clip current() const { return clip; }
    uint8_t frame() const { const uint8_t n=count(clip); return clip!=Blink && step>=n ? 2*n-2-step : step; }
    uint8_t alpha(uint32_t now) const
    {
        if(phase!=Fade) return 255;
        const uint32_t elapsed=now-fadeStart;
        if(elapsed<entryDelayMs) return 0;
        const uint32_t fadeElapsed=elapsed-entryDelayMs;
        return fadeElapsed>=entryFadeMs ? 255 : static_cast<uint8_t>(fadeElapsed*255/entryFadeMs);
    }
    bool tick(uint32_t now, bool preparation, bool speaking)
    {
        if(phase==Fade && now-fadeStart<entryDelayMs && !closing) { last=now; return false; }
        if(done || now-last<100) return false;
        last=now-last<200 ? last+100 : now;
        if(++step < length(clip)) return true;
        step=0;
        if(closing) { if(phase==Exit) done=true; else { phase=Exit; clip=Goodbye; } return true; }
        if(phase==Fade)
        {
            if(now-fadeStart<entryDelayMs+entryFadeMs) return true;
            phase=Hello; clip=Goodbye; return true;
        }
        if(phase==Hello) { phase=HelloSmile; clip=Smile; return true; }
        if(phase==HelloSmile) { phase=Normal; clip=Idle; return true; }
        if(speaking) { phase=Normal; clip=Idle; prepIndex=0; return true; }
        if(preparation)
        {
            static const Clip sequence[]={Thinking,Idle,Blink,Idle};
            if(phase!=Preparing) { phase=Preparing; prepIndex=0; }
            clip=sequence[prepIndex]; prepIndex=(prepIndex+1)%4; return true;
        }
        if(phase==Preparing) { phase=Normal; clip=Idle; return true; }
        if(clip!=Idle) { clip=Idle; return true; }
        if(static_cast<int32_t>(now-coffeeDue)>=0) { clip=Coffee; coffeeDue=now+60000+random()%15001; return true; }
        // Per completed Idle: Blink 45%, looks 30%, Thinking 3%, Idle 22%.
        const uint32_t choice=random()%100;
        clip=choice<45 ? Blink : choice<75 ? static_cast<Clip>(LookUp+random()%4) : choice<78 ? Thinking : Idle;
        return true;
    }
    static uint8_t count(Clip c) { static const uint8_t counts[]={6,6,12,5,8,9,19,22,26,11}; return counts[c]; }
    static uint8_t length(Clip c) { return c==Blink ? count(c) : 2*count(c)-1; }
private:
    static constexpr uint32_t entryDelayMs=1000;
    static constexpr uint32_t entryFadeMs=2000;
    enum Phase { Fade,Hello,HelloSmile,Normal,Preparing,Exit };
    Phase phase=Normal;
    Clip clip=Idle;
    uint8_t step=0,prepIndex=0;
    uint32_t last=0,fadeStart=0,coffeeDue=0,rng=1;
    bool closing=false,done=false;
    uint32_t random() { rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return rng; }
};
