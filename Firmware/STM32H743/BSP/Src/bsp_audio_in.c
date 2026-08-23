#include "bsp_audio_in.h"

#include "pdm_decimator.h"
#include "sai.h"

#define BSP_AUDIO_IN_DECIMATION       128U
#define BSP_AUDIO_IN_PDM_HALF_BYTES \
    ((BSP_AUDIO_IN_BLOCK_FRAMES * BSP_AUDIO_IN_DECIMATION * \
      BSP_AUDIO_IN_CHANNELS) / 8U)
#define BSP_AUDIO_IN_PDM_BUFFER_BYTES (2U * BSP_AUDIO_IN_PDM_HALF_BYTES)

__attribute__((section(".dma_buffer.audio_in"), aligned(32)))
static uint8_t bsp_audio_in_pdm[BSP_AUDIO_IN_PDM_BUFFER_BYTES];

static PDM_Decimator_t bsp_audio_in_filter[BSP_AUDIO_IN_CHANNELS];
static volatile uint8_t bsp_audio_in_ready_mask;
static volatile uint32_t bsp_audio_in_overruns;
static volatile uint32_t bsp_audio_in_last_hal_error;
static volatile bool bsp_audio_in_running;
static BSP_AUDIO_IN_Signal_t bsp_audio_in_signal;

BSP_AUDIO_IN_Status_t BSP_AUDIO_IN_Init(void)
{
    for (uint32_t channel = 0U; channel < BSP_AUDIO_IN_CHANNELS; ++channel)
    {
        PDM_Decimator_Init(&bsp_audio_in_filter[channel]);
    }
    bsp_audio_in_ready_mask = 0U;
    bsp_audio_in_overruns = 0U;
    bsp_audio_in_last_hal_error = HAL_SAI_ERROR_NONE;
    bsp_audio_in_running = false;
    bsp_audio_in_signal = NULL;
    return BSP_AUDIO_IN_OK;
}

BSP_AUDIO_IN_Status_t BSP_AUDIO_IN_Start(BSP_AUDIO_IN_Signal_t signal)
{
    bsp_audio_in_signal = signal;
    bsp_audio_in_ready_mask = 0U;
    bsp_audio_in_last_hal_error = HAL_SAI_ERROR_NONE;
    if (HAL_SAI_Receive_DMA(&hsai_BlockA1,
                            bsp_audio_in_pdm,
                            sizeof(bsp_audio_in_pdm) / sizeof(uint16_t)) != HAL_OK)
    {
        bsp_audio_in_last_hal_error = hsai_BlockA1.ErrorCode;
        return BSP_AUDIO_IN_ERROR_DMA_START;
    }
    bsp_audio_in_running = true;
    return BSP_AUDIO_IN_OK;
}

BSP_AUDIO_IN_Status_t BSP_AUDIO_IN_Stop(void)
{
    if (bsp_audio_in_running)
    {
        (void)HAL_SAI_DMAStop(&hsai_BlockA1);
    }
    bsp_audio_in_running = false;
    bsp_audio_in_ready_mask = 0U;
    return BSP_AUDIO_IN_OK;
}

BSP_AUDIO_IN_Status_t BSP_AUDIO_IN_ProcessNextBlock(int16_t *pcm_stereo,
                                                    size_t frame_capacity,
                                                    size_t *frames_written)
{
    uint8_t half;
    uint8_t mask;

    if ((pcm_stereo == NULL) || (frames_written == NULL) ||
        (frame_capacity < BSP_AUDIO_IN_BLOCK_FRAMES))
    {
        return BSP_AUDIO_IN_ERROR_INVALID_ARGUMENT;
    }

    mask = bsp_audio_in_ready_mask;
    if ((mask & 0x01U) != 0U)
    {
        half = 0U;
    }
    else if ((mask & 0x02U) != 0U)
    {
        half = 1U;
    }
    else
    {
        *frames_written = 0U;
        return BSP_AUDIO_IN_NO_DATA;
    }
    bsp_audio_in_ready_mask &= (uint8_t)~(1U << half);

    const uint8_t *pdm = &bsp_audio_in_pdm[half * BSP_AUDIO_IN_PDM_HALF_BYTES];
    SCB_InvalidateDCache_by_Addr((uint32_t *)(void *)pdm,
                                BSP_AUDIO_IN_PDM_HALF_BYTES);

    for (uint32_t channel = 0U; channel < BSP_AUDIO_IN_CHANNELS; ++channel)
    {
        const size_t produced = PDM_Decimator_Process(
            &bsp_audio_in_filter[channel], pdm, BSP_AUDIO_IN_PDM_HALF_BYTES,
            channel, BSP_AUDIO_IN_CHANNELS, BSP_AUDIO_IN_DECIMATION, 96,
            &pcm_stereo[channel], BSP_AUDIO_IN_CHANNELS);
        if (produced != BSP_AUDIO_IN_BLOCK_FRAMES)
        {
            *frames_written = 0U;
            return BSP_AUDIO_IN_ERROR_DMA;
        }
    }
    *frames_written = BSP_AUDIO_IN_BLOCK_FRAMES;
    return BSP_AUDIO_IN_OK;
}

uint32_t BSP_AUDIO_IN_GetOverrunCount(void) { return bsp_audio_in_overruns; }
uint32_t BSP_AUDIO_IN_GetLastHALerror(void) { return bsp_audio_in_last_hal_error; }
bool BSP_AUDIO_IN_IsRunning(void) { return bsp_audio_in_running; }

static void BSP_AUDIO_IN_MarkReady(uint8_t bit)
{
    if ((bsp_audio_in_ready_mask & bit) != 0U)
    {
        ++bsp_audio_in_overruns;
    }
    bsp_audio_in_ready_mask |= bit;
    if (bsp_audio_in_signal != NULL)
    {
        bsp_audio_in_signal();
    }
}

void BSP_AUDIO_IN_HalfTransferCallback(void) { BSP_AUDIO_IN_MarkReady(0x01U); }
void BSP_AUDIO_IN_TransferCompleteCallback(void) { BSP_AUDIO_IN_MarkReady(0x02U); }

void BSP_AUDIO_IN_ErrorCallback(uint32_t hal_error)
{
    bsp_audio_in_last_hal_error = hal_error;
    bsp_audio_in_running = false;
    if (bsp_audio_in_signal != NULL)
    {
        bsp_audio_in_signal();
    }
}
