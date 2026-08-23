#include "audio_dsp.h"

#include <limits.h>
#include <stdbool.h>
#include <string.h>

#define AUDIO_DSP_GAIN_ONE_Q12       4096U
#define AUDIO_DSP_GAIN_INITIAL_Q12   (4U * AUDIO_DSP_GAIN_ONE_Q12)
#define AUDIO_DSP_GAIN_MAX_Q12       (10U * AUDIO_DSP_GAIN_ONE_Q12)
#define AUDIO_DSP_TARGET_PEAK        16000U
#define AUDIO_DSP_INITIAL_NOISE      180U
#define AUDIO_DSP_MIN_GATE           250U

static uint32_t AUDIO_DSP_Abs16(int16_t value)
{
    return (uint32_t)((value < 0) ? -(int32_t)value : value);
}

static int16_t AUDIO_DSP_Saturate16(int32_t value)
{
    if (value > INT16_MAX) { return INT16_MAX; }
    if (value < INT16_MIN) { return INT16_MIN; }
    return (int16_t)value;
}

void AUDIO_DSP_Init(AUDIO_DSP_State_t *state)
{
    if (state != NULL)
    {
        memset(state, 0, sizeof(*state));
        state->noise_floor = AUDIO_DSP_INITIAL_NOISE;
        state->gain_q12 = AUDIO_DSP_GAIN_INITIAL_Q12;
        /* Default for the opposite-edge PDM pair. Block energy below updates
           the effective polarity only when one combination is clearly better. */
        state->right_polarity = -1;
    }
}

size_t AUDIO_DSP_ProcessStereo16(AUDIO_DSP_State_t *state,
                                 const int16_t *stereo,
                                 size_t frames,
                                 int16_t *mono,
                                 AUDIO_DSP_Metrics_t *metrics)
{
    uint32_t input_peak = 0U;
    uint32_t output_peak = 0U;
    uint64_t sum_energy = 0U;
    uint64_t difference_energy = 0U;

    if ((state == NULL) || (stereo == NULL) || (mono == NULL))
    {
        return 0U;
    }

    for (size_t frame = 0U; frame < frames; ++frame)
    {
        const int32_t left = stereo[2U * frame];
        const int32_t right = stereo[(2U * frame) + 1U];
        const int32_t sum = (left + right) / 2;
        const int32_t difference = (left - right) / 2;
        sum_energy += (uint64_t)((int64_t)sum * sum);
        difference_energy += (uint64_t)((int64_t)difference * difference);
    }
    if (sum_energy > ((difference_energy * 5U) / 4U))
    {
        state->right_polarity = 1;
    }
    else if (difference_energy > ((sum_energy * 5U) / 4U))
    {
        state->right_polarity = -1;
    }

    for (size_t frame = 0U; frame < frames; ++frame)
    {
        const int32_t left = stereo[2U * frame];
        const int32_t right = stereo[(2U * frame) + 1U];
        state->correlation += (int64_t)left * right;
        ++state->correlation_samples;

        const int32_t combined =
            (left + ((int32_t)state->right_polarity * right)) / 2;
        const uint32_t magnitude =
            (uint32_t)((combined < 0) ? -combined : combined);
        if (magnitude > input_peak) { input_peak = magnitude; }

        /* First-order DC blocker followed by a light two-sample average. */
        const int32_t high_pass = combined - state->previous_input +
            (int32_t)(((int64_t)32604 * state->high_pass_state) >> 15);
        state->previous_input = combined;
        state->high_pass_state = high_pass;
        const int32_t filtered = (high_pass + state->low_pass_state) / 2;
        state->low_pass_state = high_pass;
        mono[frame] = AUDIO_DSP_Saturate16(filtered);
    }

    uint32_t gate = state->noise_floor + (state->noise_floor / 2U);
    if (gate < AUDIO_DSP_MIN_GATE) { gate = AUDIO_DSP_MIN_GATE; }
    const bool speech = input_peak > gate;
    if (!speech)
    {
        state->noise_floor = ((31U * state->noise_floor) + input_peak) / 32U;
        gate = state->noise_floor + (state->noise_floor / 2U);
        if (gate < AUDIO_DSP_MIN_GATE) { gate = AUDIO_DSP_MIN_GATE; }
    }

    uint32_t desired_gain = AUDIO_DSP_GAIN_ONE_Q12;
    if (speech && (input_peak != 0U))
    {
        desired_gain = (AUDIO_DSP_TARGET_PEAK * AUDIO_DSP_GAIN_ONE_Q12) /
                       input_peak;
        if (desired_gain < AUDIO_DSP_GAIN_ONE_Q12)
        {
            desired_gain = AUDIO_DSP_GAIN_ONE_Q12;
        }
        if (desired_gain > AUDIO_DSP_GAIN_MAX_Q12)
        {
            desired_gain = AUDIO_DSP_GAIN_MAX_Q12;
        }
        const uint32_t divisor =
            (desired_gain < state->gain_q12) ? 4U : 8U;
        state->gain_q12 = (uint32_t)((int32_t)state->gain_q12 +
            (((int32_t)desired_gain - (int32_t)state->gain_q12) /
             (int32_t)divisor));
    }

    for (size_t frame = 0U; frame < frames; ++frame)
    {
        int32_t sample =
            (int32_t)(((int64_t)mono[frame] * state->gain_q12) >> 12);
        if (!speech)
        {
            sample /= 4;
        }
        mono[frame] = AUDIO_DSP_Saturate16(sample);
        const uint32_t magnitude = AUDIO_DSP_Abs16(mono[frame]);
        if (magnitude > output_peak) { output_peak = magnitude; }
    }

    if (metrics != NULL)
    {
        metrics->input_peak = input_peak;
        metrics->output_peak = output_peak;
        metrics->noise_floor = state->noise_floor;
        metrics->gate_threshold = gate;
        metrics->gain_q12 = state->gain_q12;
        metrics->right_polarity = state->right_polarity;
        metrics->speech_detected = speech ? 1U : 0U;
    }
    return frames;
}
