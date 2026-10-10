#ifndef SPEECH_ANIMATION_H
#define SPEECH_ANIMATION_H
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t start_sample;
    uint32_t end_sample;
    uint8_t first_frame;
    uint8_t last_frame;
} SpeechCue;

/* Original playback: select a zero-based frame directly from the audio clock. */
static inline uint8_t SpeechAnimation_Frame(const SpeechCue *cues, size_t count,
                                           uint32_t sample, uint32_t sample_rate)
{
    if (sample_rate == 0U) return 0U;
    size_t low=0U, high=count;
    while (low<high) {
        const size_t middle=low+(high-low)/2U;
        if (cues[middle].start_sample<=sample) low=middle+1U;
        else high=middle;
    }
    uint32_t silence_start=0U;
    if (low!=0U) {
        const SpeechCue *cue=&cues[low-1U];
        if (sample<cue->end_sample) {
            const uint32_t duration=cue->end_sample-cue->start_sample;
            const uint32_t frames=cue->last_frame-cue->first_frame+1U;
            return (uint8_t)(cue->first_frame-1U+
                (uint64_t)(sample-cue->start_sample)*frames/duration);
        }
        silence_start=cue->end_sample;
    }
    uint32_t silence_ms=(uint32_t)((uint64_t)(sample-silence_start)*1000U/sample_rate);
    if (low!=0U && silence_ms<180U) return (uint8_t)(36U+silence_ms/60U);
    if (low!=0U) silence_ms-=180U;
    return (uint8_t)((silence_ms/120U)%8U);
}
#endif
