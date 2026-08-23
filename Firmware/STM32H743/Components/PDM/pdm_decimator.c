#include "pdm_decimator.h"

#include <string.h>

static int16_t PDM_Saturate16(int64_t value)
{
    if (value > INT16_MAX)
    {
        return INT16_MAX;
    }
    if (value < INT16_MIN)
    {
        return INT16_MIN;
    }
    return (int16_t)value;
}

void PDM_Decimator_Init(PDM_Decimator_t *state)
{
    if (state != NULL)
    {
        memset(state, 0, sizeof(*state));
    }
}

size_t PDM_Decimator_Process(PDM_Decimator_t *state,
                             const uint8_t *interleaved_pdm,
                             size_t pdm_bytes,
                             size_t channel,
                             size_t channel_count,
                             uint16_t decimation,
                             int16_t gain,
                             int16_t *pcm,
                             size_t pcm_stride)
{
    size_t output_count = 0U;
    uint16_t phase = 0U;

    if ((state == NULL) || (interleaved_pdm == NULL) || (pcm == NULL) ||
        (channel >= channel_count) || (channel_count == 0U) ||
        (decimation == 0U) || (pcm_stride == 0U))
    {
        return 0U;
    }

    /* SAI PDM mode stores one byte per microphone in turn. Within each byte
       the first received PDM bit is the MSB, matching SAI_FIRSTBIT_MSB. */
    for (size_t byte_index = channel; byte_index < pdm_bytes;
         byte_index += channel_count)
    {
        const uint8_t bits = interleaved_pdm[byte_index];
        for (uint32_t bit = 0U; bit < 8U; ++bit)
        {
            const int32_t sample = ((bits & (0x80U >> bit)) != 0U) ? 1 : -1;
            state->integrator[0] += sample;
            state->integrator[1] += state->integrator[0];
            state->integrator[2] += state->integrator[1];

            ++phase;
            if (phase == decimation)
            {
                int64_t value = state->integrator[2];
                phase = 0U;
                for (uint32_t stage = 0U; stage < 3U; ++stage)
                {
                    const int64_t delayed = state->comb_delay[stage];
                    state->comb_delay[stage] = value;
                    value -= delayed;
                }

                /* A 3rd-order CIC at R=128 has gain 2^21. Convert to a
                   conservative 16-bit voice level, then remove slow DC. */
                value = (value * gain) >> 12;
                state->dc_estimate +=
                    ((int32_t)value - state->dc_estimate) >> 8;
                value -= state->dc_estimate;
                pcm[output_count * pcm_stride] = PDM_Saturate16(value);
                ++output_count;
            }
        }
    }

    return output_count;
}
