#ifndef PDM_DECIMATOR_H
#define PDM_DECIMATOR_H

#include <stddef.h>
#include <stdint.h>

typedef struct
{
    int64_t integrator[3];
    int64_t comb_delay[3];
    int32_t dc_estimate;
} PDM_Decimator_t;

void PDM_Decimator_Init(PDM_Decimator_t *state);
size_t PDM_Decimator_Process(PDM_Decimator_t *state,
                             const uint8_t *interleaved_pdm,
                             size_t pdm_bytes,
                             size_t channel,
                             size_t channel_count,
                             uint16_t decimation,
                             int16_t gain,
                             int16_t *pcm,
                             size_t pcm_stride);

#endif
