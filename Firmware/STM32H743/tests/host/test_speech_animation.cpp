#include <assert.h>
#include <stdio.h>
#include <gui/common/AvatarAnimation.hpp>
#include "speech_animation.h"
#include "chat_test_audio.h"
#include "chat_test_timeline.h"
int main()
{
    const SpeechCue cues[]={{4800,14400,9,14},{28800,38400,14,23},{48000,96000,23,37}};
    assert(SpeechAnimation_Frame(cues,3,0,48000)==0);
    assert(SpeechAnimation_Frame(cues,3,4800,48000)==8);
    assert(SpeechAnimation_Frame(cues,3,14399,48000)==13);
    assert(SpeechAnimation_Frame(cues,3,14400,48000)==36);
    assert(SpeechAnimation_Frame(cues,3,17280,48000)==37);
    assert(SpeechAnimation_Frame(cues,3,20160,48000)==38);
    assert(SpeechAnimation_Frame(cues,3,23040,48000)==0);
    assert(SpeechAnimation_Frame(cues,3,28800,48000)==13);
    assert(SpeechAnimation_Frame(cues,3,38399,48000)==22);
    assert(SpeechAnimation_Frame(cues,3,48000,48000)==22);
    assert(SpeechAnimation_Frame(cues,3,95999,48000)==36);
    assert(SpeechAnimation_Frame(nullptr,0,5760,48000)==1);
    assert(SpeechAnimation_Frame(nullptr,0,46080,48000)==0);
    assert(SpeechAnimation_Frame(nullptr,0,1,0)==0);
    const uint8_t jumped=SpeechAnimation_Frame(cues,3,72000,48000);
    assert(jumped==29);
    for (uint32_t s=0;s<CHAT_TEST_SAMPLE_COUNT+48000U;s+=48U)
        assert(SpeechAnimation_Frame(chat_test_cues,CHAT_TEST_CUE_COUNT,s,48000)<39);
    for (unsigned i=0;i<CHAT_TEST_CUE_COUNT;++i) {
        const SpeechCue& c=chat_test_cues[i];
        assert(c.start_sample<c.end_sample && c.end_sample<=CHAT_TEST_SAMPLE_COUNT);
        assert(i==0 || chat_test_cues[i-1].end_sample<=c.start_sample);
        assert(SpeechAnimation_Frame(chat_test_cues,CHAT_TEST_CUE_COUNT,c.start_sample,48000)==c.first_frame-1);
        assert(SpeechAnimation_Frame(chat_test_cues,CHAT_TEST_CUE_COUNT,c.end_sample-1,48000)==c.last_frame-1);
    }
    AvatarAnimation avatar;
    avatar.enterSpeech(0); avatar.setSpeechFrame(jumped); avatar.tick(3000,false,false);
    assert(avatar.alpha(3000)==255 && avatar.current()==AvatarAnimation::Speak && avatar.frame()==jumped);
    avatar.setSpeechFrame(0); avatar.tick(9000,false,false);
    assert(avatar.current()==AvatarAnimation::Speak && avatar.frame()==0);
    avatar.requestClose(); avatar.tick(9001,false,false);
    assert(avatar.current()==AvatarAnimation::Goodbye);
    for (uint32_t t=9101;t<14000;t+=100) avatar.tick(t,false,false);
    assert(avatar.closed());
    avatar.enterSpeech(15000); avatar.setSpeechFrame(7); avatar.tick(15001,false,false);
    assert(!avatar.closed() && avatar.current()==AvatarAnimation::Speak && avatar.frame()==7);
    avatar.requestClose(); avatar.tick(15002,false,false);
    assert(avatar.current()==AvatarAnimation::Goodbye);
    puts("PASS: original speech ranges, 120 ms idle, 60 ms closing, audio seeking and re-entry");
}
