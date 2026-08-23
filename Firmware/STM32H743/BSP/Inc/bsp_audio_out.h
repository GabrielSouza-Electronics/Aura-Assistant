#ifndef BSP_AUDIO_OUT_H
#define BSP_AUDIO_OUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "audio_sfx.h"

typedef enum
{
    BSP_AUDIO_OUT_OK = 0,
    BSP_AUDIO_OUT_ERROR_INVALID_ARGUMENT,
    BSP_AUDIO_OUT_ERROR_BUSY,
    BSP_AUDIO_OUT_ERROR_RENDER,
    BSP_AUDIO_OUT_ERROR_DMA_START,
    BSP_AUDIO_OUT_ERROR_TIMEOUT,
    BSP_AUDIO_OUT_ERROR_DMA
} BSP_AUDIO_OUT_Status_t;

typedef void (*BSP_AUDIO_OUT_PrepareWait_t)(void);
typedef bool (*BSP_AUDIO_OUT_Wait_t)(uint32_t timeout_ms);
typedef void (*BSP_AUDIO_OUT_Signal_t)(void);

BSP_AUDIO_OUT_Status_t BSP_AUDIO_OUT_Init(void);
void BSP_AUDIO_OUT_SetSynchronizationHooks(BSP_AUDIO_OUT_PrepareWait_t prepare_wait,
                                           BSP_AUDIO_OUT_Wait_t wait,
                                           BSP_AUDIO_OUT_Signal_t signal);
BSP_AUDIO_OUT_Status_t BSP_AUDIO_OUT_PlayEffectBlocking(AUDIO_SFX_Id_t effect,
                                                        uint32_t timeout_ms);
BSP_AUDIO_OUT_Status_t BSP_AUDIO_OUT_PlayPCM16kStereoBlocking(
    const int16_t *pcm_stereo, size_t frame_count, uint32_t timeout_ms);
BSP_AUDIO_OUT_Status_t BSP_AUDIO_OUT_StartEchoStream(void);
BSP_AUDIO_OUT_Status_t BSP_AUDIO_OUT_QueueEchoBlock(
    const int16_t *pcm_stereo, size_t frame_count);
BSP_AUDIO_OUT_Status_t BSP_AUDIO_OUT_StopEchoStream(void);
uint32_t BSP_AUDIO_OUT_GetEchoUnderrunCount(void);
bool BSP_AUDIO_OUT_IsBusy(void);
uint32_t BSP_AUDIO_OUT_GetLastHALerror(void);
void BSP_AUDIO_OUT_HalfTransferCallback(void);
void BSP_AUDIO_OUT_TransferCompleteCallback(void);
void BSP_AUDIO_OUT_ErrorCallback(uint32_t hal_error);

#endif
