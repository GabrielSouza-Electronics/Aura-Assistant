#ifndef AUDIO_DSP_H
#define AUDIO_DSP_H

#include <stddef.h>
#include <stdint.h>

typedef struct
{
    int32_t previous_input;
    int32_t high_pass_state;
    int32_t low_pass_state;
    uint32_t noise_floor;
    uint32_t gain_q12;
    int64_t correlation;
    uint32_t correlation_samples;
    int8_t right_polarity;
} AUDIO_DSP_State_t;

typedef struct
{
    uint32_t input_peak;
    uint32_t output_peak;
    uint32_t noise_floor;
    uint32_t gate_threshold;
    uint32_t gain_q12;
    int8_t right_polarity;
    uint8_t speech_detected;
} AUDIO_DSP_Metrics_t;

void AUDIO_DSP_Init(AUDIO_DSP_State_t *state);
size_t AUDIO_DSP_ProcessStereo16(AUDIO_DSP_State_t *state,
                                 const int16_t *stereo,
                                 size_t frames,
                                 int16_t *mono,
                                 AUDIO_DSP_Metrics_t *metrics);

#endif
