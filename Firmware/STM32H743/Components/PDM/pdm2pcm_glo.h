#ifndef PDM2PCM_GLO_H
#define PDM2PCM_GLO_H

#include <stdint.h>

#define PDM_FILTER_ENDIANNESS_LE  ((uint16_t)0x0000)
#define PDM_FILTER_BIT_ORDER_MSB  ((uint16_t)0x0001)
#define PDM_FILTER_DEC_FACTOR_128 ((uint16_t)0x0004)
#define PDM2PCM_INTERNAL_MEMORY_SIZE 16

typedef struct
{
    uint16_t bit_order;
    uint16_t endianness;
    uint32_t high_pass_tap;
    uint16_t in_ptr_channels;
    uint16_t out_ptr_channels;
    uint32_t pInternalMemory[PDM2PCM_INTERNAL_MEMORY_SIZE];
} PDM_Filter_Handler_t;

typedef struct
{
    uint16_t decimation_factor;
    uint16_t output_samples_number;
    int16_t mic_gain;
} PDM_Filter_Config_t;

uint32_t PDM_Filter_Init(PDM_Filter_Handler_t *handler);
uint32_t PDM_Filter_setConfig(PDM_Filter_Handler_t *handler,
                              PDM_Filter_Config_t *config);
uint32_t PDM_Filter(void *data_in, void *data_out,
                    PDM_Filter_Handler_t *handler);

#endif
