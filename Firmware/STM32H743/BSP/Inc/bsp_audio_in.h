#ifndef BSP_AUDIO_IN_H
#define BSP_AUDIO_IN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BSP_AUDIO_IN_CHANNELS       2U
#define BSP_AUDIO_IN_SAMPLE_RATE_HZ 16000U
#define BSP_AUDIO_IN_BLOCK_FRAMES   128U

typedef enum
{
    BSP_AUDIO_IN_OK = 0,
    BSP_AUDIO_IN_NO_DATA,
    BSP_AUDIO_IN_ERROR_INVALID_ARGUMENT,
    BSP_AUDIO_IN_ERROR_DMA_START,
    BSP_AUDIO_IN_ERROR_DMA
} BSP_AUDIO_IN_Status_t;

typedef void (*BSP_AUDIO_IN_Signal_t)(void);

BSP_AUDIO_IN_Status_t BSP_AUDIO_IN_Init(void);
BSP_AUDIO_IN_Status_t BSP_AUDIO_IN_Start(BSP_AUDIO_IN_Signal_t signal);
BSP_AUDIO_IN_Status_t BSP_AUDIO_IN_Stop(void);
BSP_AUDIO_IN_Status_t BSP_AUDIO_IN_ProcessNextBlock(int16_t *pcm_stereo,
                                                    size_t frame_capacity,
                                                    size_t *frames_written);
uint32_t BSP_AUDIO_IN_GetOverrunCount(void);
uint32_t BSP_AUDIO_IN_GetLastHALerror(void);
bool BSP_AUDIO_IN_IsRunning(void);
void BSP_AUDIO_IN_HalfTransferCallback(void);
void BSP_AUDIO_IN_TransferCompleteCallback(void);
void BSP_AUDIO_IN_ErrorCallback(uint32_t hal_error);

#endif
