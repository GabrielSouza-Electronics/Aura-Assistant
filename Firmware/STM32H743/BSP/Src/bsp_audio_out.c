#include "bsp_audio_out.h"

#include "i2s.h"
#include "main.h"

#define BSP_AUDIO_OUT_MAX_DURATION_MS 500U
#define BSP_AUDIO_OUT_STEREO_SAMPLES \
    ((AUDIO_SFX_SAMPLE_RATE_HZ * BSP_AUDIO_OUT_MAX_DURATION_MS * 2U) / 1000U)
#define BSP_AUDIO_OUT_ECHO_INPUT_FRAMES 128U
#define BSP_AUDIO_OUT_ECHO_HALF_SAMPLES \
    (BSP_AUDIO_OUT_ECHO_INPUT_FRAMES * 3U * 2U)
#define BSP_AUDIO_OUT_ECHO_SAMPLES (2U * BSP_AUDIO_OUT_ECHO_HALF_SAMPLES)
#define BSP_AUDIO_OUT_ECHO_GAIN 1

__attribute__((section(".dma_buffer.audio_out"), aligned(32)))
static int16_t bsp_audio_out_dma_buffer[BSP_AUDIO_OUT_STEREO_SAMPLES];

static volatile bool bsp_audio_out_busy;
static volatile bool bsp_audio_out_dma_error;
static volatile bool bsp_audio_out_keep_enabled;
static volatile uint32_t bsp_audio_out_last_hal_error;
static volatile bool bsp_audio_out_echo_stream;
static volatile uint8_t bsp_audio_out_echo_writable;
static volatile uint32_t bsp_audio_out_echo_underruns;
static BSP_AUDIO_OUT_PrepareWait_t bsp_audio_out_prepare_wait;
static BSP_AUDIO_OUT_Wait_t bsp_audio_out_wait;
static BSP_AUDIO_OUT_Signal_t bsp_audio_out_signal;

static BSP_AUDIO_OUT_Status_t BSP_AUDIO_OUT_TransmitBuffer(size_t sample_count,
                                                           uint32_t timeout_ms,
                                                           bool keep_enabled)
{
    const uint32_t start_tick = HAL_GetTick();

    bsp_audio_out_dma_error = false;
    bsp_audio_out_last_hal_error = HAL_I2S_ERROR_NONE;
    bsp_audio_out_keep_enabled = keep_enabled;
    bsp_audio_out_busy = true;

    if (bsp_audio_out_prepare_wait != NULL)
    {
        bsp_audio_out_prepare_wait();
    }

    SCB_CleanDCache_by_Addr((uint32_t *)bsp_audio_out_dma_buffer,
                           (int32_t)(sample_count * sizeof(int16_t)));
    HAL_GPIO_WritePin(I2S_SDMODE_GPIO_Port, I2S_SDMODE_Pin, GPIO_PIN_SET);
    for (volatile uint32_t delay = 0U; delay < 10000U; ++delay)
    {
        __NOP();
    }

    if (HAL_I2S_Transmit_DMA(&hi2s1,
                             (uint16_t *)(void *)bsp_audio_out_dma_buffer,
                             (uint16_t)sample_count) != HAL_OK)
    {
        bsp_audio_out_busy = false;
        bsp_audio_out_last_hal_error = hi2s1.ErrorCode;
        HAL_GPIO_WritePin(I2S_SDMODE_GPIO_Port, I2S_SDMODE_Pin, GPIO_PIN_RESET);
        return BSP_AUDIO_OUT_ERROR_DMA_START;
    }

    if (bsp_audio_out_wait != NULL)
    {
        if (!bsp_audio_out_wait(timeout_ms) || bsp_audio_out_busy)
        {
            (void)HAL_I2S_DMAStop(&hi2s1);
            bsp_audio_out_busy = false;
            bsp_audio_out_keep_enabled = false;
            HAL_GPIO_WritePin(I2S_SDMODE_GPIO_Port, I2S_SDMODE_Pin, GPIO_PIN_RESET);
            return BSP_AUDIO_OUT_ERROR_TIMEOUT;
        }
    }
    else
    {
        while (bsp_audio_out_busy)
        {
            if ((HAL_GetTick() - start_tick) >= timeout_ms)
            {
                (void)HAL_I2S_DMAStop(&hi2s1);
                bsp_audio_out_busy = false;
                bsp_audio_out_keep_enabled = false;
                HAL_GPIO_WritePin(I2S_SDMODE_GPIO_Port, I2S_SDMODE_Pin, GPIO_PIN_RESET);
                return BSP_AUDIO_OUT_ERROR_TIMEOUT;
            }
            HAL_Delay(1U);
        }
    }

    return bsp_audio_out_dma_error ? BSP_AUDIO_OUT_ERROR_DMA
                                   : BSP_AUDIO_OUT_OK;
}

void BSP_AUDIO_OUT_SetSynchronizationHooks(BSP_AUDIO_OUT_PrepareWait_t prepare_wait,
                                           BSP_AUDIO_OUT_Wait_t wait,
                                           BSP_AUDIO_OUT_Signal_t signal)
{
    bsp_audio_out_prepare_wait = prepare_wait;
    bsp_audio_out_wait = wait;
    bsp_audio_out_signal = signal;
}

BSP_AUDIO_OUT_Status_t BSP_AUDIO_OUT_Init(void)
{
    bsp_audio_out_busy = false;
    bsp_audio_out_dma_error = false;
    bsp_audio_out_keep_enabled = false;
    bsp_audio_out_last_hal_error = HAL_I2S_ERROR_NONE;
    bsp_audio_out_echo_stream = false;
    bsp_audio_out_echo_writable = 0U;
    bsp_audio_out_echo_underruns = 0U;
    HAL_GPIO_WritePin(I2S_SDMODE_GPIO_Port, I2S_SDMODE_Pin, GPIO_PIN_RESET);
    return BSP_AUDIO_OUT_OK;
}

static void BSP_AUDIO_OUT_RenderEchoHalf(size_t half,
                                         const int16_t *pcm_stereo,
                                         size_t frame_count)
{
    size_t output_index = half * BSP_AUDIO_OUT_ECHO_HALF_SAMPLES;
    for (size_t frame = 0U; frame < frame_count; ++frame)
    {
        int32_t monitor =
            (int32_t)pcm_stereo[2U * frame] * BSP_AUDIO_OUT_ECHO_GAIN;
        if (monitor > INT16_MAX)
        {
            monitor = INT16_MAX;
        }
        else if (monitor < INT16_MIN)
        {
            monitor = INT16_MIN;
        }

        /* The two PDM microphones use opposite clock edges and may decode
           with opposite polarity. Never feed L+R cancellation into a mono
           MAX98357A validation path: monitor mic 0 on both I2S channels. */
        for (uint32_t repeat = 0U; repeat < 3U; ++repeat)
        {
            const int16_t sample = (int16_t)monitor;
            bsp_audio_out_dma_buffer[output_index++] = sample;
            bsp_audio_out_dma_buffer[output_index++] = sample;
        }
    }
}

BSP_AUDIO_OUT_Status_t BSP_AUDIO_OUT_StartEchoStream(void)
{
    if (bsp_audio_out_busy)
    {
        return BSP_AUDIO_OUT_ERROR_BUSY;
    }

    for (size_t index = 0U; index < BSP_AUDIO_OUT_ECHO_SAMPLES; ++index)
    {
        bsp_audio_out_dma_buffer[index] = 0;
    }
    SCB_CleanDCache_by_Addr((uint32_t *)bsp_audio_out_dma_buffer,
                           BSP_AUDIO_OUT_ECHO_SAMPLES * sizeof(int16_t));

    /* Sound effects use normal DMA. Echo requires capture and playback to run
       concurrently, so switch only this validation session to circular DMA. */
    (void)HAL_DMA_DeInit(hi2s1.hdmatx);
    hi2s1.hdmatx->Init.Mode = DMA_CIRCULAR;
    if (HAL_DMA_Init(hi2s1.hdmatx) != HAL_OK)
    {
        bsp_audio_out_last_hal_error = hi2s1.ErrorCode;
        return BSP_AUDIO_OUT_ERROR_DMA_START;
    }

    bsp_audio_out_echo_stream = true;
    bsp_audio_out_echo_writable = 0U;
    bsp_audio_out_echo_underruns = 0U;
    bsp_audio_out_busy = true;
    HAL_GPIO_WritePin(I2S_SDMODE_GPIO_Port, I2S_SDMODE_Pin, GPIO_PIN_SET);
    for (volatile uint32_t delay = 0U; delay < 10000U; ++delay)
    {
        __NOP();
    }
    if (HAL_I2S_Transmit_DMA(&hi2s1,
                             (uint16_t *)(void *)bsp_audio_out_dma_buffer,
                             BSP_AUDIO_OUT_ECHO_SAMPLES) != HAL_OK)
    {
        bsp_audio_out_echo_stream = false;
        bsp_audio_out_busy = false;
        bsp_audio_out_last_hal_error = hi2s1.ErrorCode;
        HAL_GPIO_WritePin(I2S_SDMODE_GPIO_Port, I2S_SDMODE_Pin, GPIO_PIN_RESET);
        return BSP_AUDIO_OUT_ERROR_DMA_START;
    }
    return BSP_AUDIO_OUT_OK;
}

BSP_AUDIO_OUT_Status_t BSP_AUDIO_OUT_QueueEchoBlock(
    const int16_t *pcm_stereo, size_t frame_count)
{
    uint8_t writable;
    size_t half;

    if ((pcm_stereo == NULL) ||
        (frame_count != BSP_AUDIO_OUT_ECHO_INPUT_FRAMES))
    {
        return BSP_AUDIO_OUT_ERROR_INVALID_ARGUMENT;
    }
    if (!bsp_audio_out_echo_stream)
    {
        return BSP_AUDIO_OUT_ERROR_BUSY;
    }

    writable = bsp_audio_out_echo_writable;
    if ((writable & 0x01U) != 0U)
    {
        half = 0U;
    }
    else if ((writable & 0x02U) != 0U)
    {
        half = 1U;
    }
    else
    {
        return BSP_AUDIO_OUT_ERROR_BUSY;
    }
    bsp_audio_out_echo_writable &= (uint8_t)~(1U << half);
    BSP_AUDIO_OUT_RenderEchoHalf(half, pcm_stereo, frame_count);
    SCB_CleanDCache_by_Addr(
        (uint32_t *)&bsp_audio_out_dma_buffer[half * BSP_AUDIO_OUT_ECHO_HALF_SAMPLES],
        BSP_AUDIO_OUT_ECHO_HALF_SAMPLES * sizeof(int16_t));
    return BSP_AUDIO_OUT_OK;
}

BSP_AUDIO_OUT_Status_t BSP_AUDIO_OUT_StopEchoStream(void)
{
    if (bsp_audio_out_echo_stream)
    {
        (void)HAL_I2S_DMAStop(&hi2s1);
    }
    bsp_audio_out_echo_stream = false;
    bsp_audio_out_busy = false;
    HAL_GPIO_WritePin(I2S_SDMODE_GPIO_Port, I2S_SDMODE_Pin, GPIO_PIN_RESET);
    return BSP_AUDIO_OUT_OK;
}

uint32_t BSP_AUDIO_OUT_GetEchoUnderrunCount(void)
{
    return bsp_audio_out_echo_underruns;
}

BSP_AUDIO_OUT_Status_t BSP_AUDIO_OUT_PlayEffectBlocking(AUDIO_SFX_Id_t effect,
                                                        uint32_t timeout_ms)
{
    size_t sample_count;

    if (effect >= AUDIO_SFX_COUNT)
    {
        return BSP_AUDIO_OUT_ERROR_INVALID_ARGUMENT;
    }
    if (bsp_audio_out_busy)
    {
        return BSP_AUDIO_OUT_ERROR_BUSY;
    }
    if (effect == AUDIO_SFX_WAKE)
    {
        const int16_t *voice_pcm = AUDIO_SFX_GetStartupVoice(&sample_count);
        size_t input_offset = 0U;

        while (input_offset < sample_count)
        {
            const size_t remaining = sample_count - input_offset;
            const size_t input_chunk =
                (remaining > (BSP_AUDIO_OUT_STEREO_SAMPLES / 6U))
                    ? (BSP_AUDIO_OUT_STEREO_SAMPLES / 6U)
                    : remaining;
            size_t output_index = 0U;

            for (size_t input_index = 0U; input_index < input_chunk; ++input_index)
            {
                /* Conservative -6 dB level for first validation on the 3 W amp. */
                const int16_t sample =
                    (int16_t)(voice_pcm[input_offset + input_index] / 2);
                for (uint32_t repeat = 0U; repeat < 3U; ++repeat)
                {
                    bsp_audio_out_dma_buffer[output_index++] = sample;
                    bsp_audio_out_dma_buffer[output_index++] = sample;
                }
            }

            input_offset += input_chunk;
            {
                const BSP_AUDIO_OUT_Status_t status = BSP_AUDIO_OUT_TransmitBuffer(
                    output_index, timeout_ms, input_offset < sample_count);
                if (status != BSP_AUDIO_OUT_OK)
                {
                    return status;
                }
            }
        }
        return BSP_AUDIO_OUT_OK;
    }

    sample_count = AUDIO_SFX_RenderStereo(
        effect, bsp_audio_out_dma_buffer, BSP_AUDIO_OUT_STEREO_SAMPLES);
    if ((sample_count == 0U) || (sample_count > UINT16_MAX))
    {
        return BSP_AUDIO_OUT_ERROR_RENDER;
    }
    return BSP_AUDIO_OUT_TransmitBuffer(sample_count, timeout_ms, false);
}

BSP_AUDIO_OUT_Status_t BSP_AUDIO_OUT_PlayPCM16kStereoBlocking(
    const int16_t *pcm_stereo, size_t frame_count, uint32_t timeout_ms)
{
    size_t output_index = 0U;

    if ((pcm_stereo == NULL) || (frame_count == 0U) ||
        (frame_count > (BSP_AUDIO_OUT_STEREO_SAMPLES / 6U)))
    {
        return BSP_AUDIO_OUT_ERROR_INVALID_ARGUMENT;
    }
    if (bsp_audio_out_busy)
    {
        return BSP_AUDIO_OUT_ERROR_BUSY;
    }

    /* I2S1 is fixed at 48 kHz. Repeat each 16 kHz stereo frame three times;
       this intentionally simple integer conversion is adequate for loopback. */
    for (size_t frame = 0U; frame < frame_count; ++frame)
    {
        const int16_t left = pcm_stereo[2U * frame];
        const int16_t right = pcm_stereo[(2U * frame) + 1U];
        for (uint32_t repeat = 0U; repeat < 3U; ++repeat)
        {
            bsp_audio_out_dma_buffer[output_index++] = left;
            bsp_audio_out_dma_buffer[output_index++] = right;
        }
    }
    return BSP_AUDIO_OUT_TransmitBuffer(output_index, timeout_ms, true);
}

bool BSP_AUDIO_OUT_IsBusy(void)
{
    return bsp_audio_out_busy;
}

uint32_t BSP_AUDIO_OUT_GetLastHALerror(void)
{
    return bsp_audio_out_last_hal_error;
}

void BSP_AUDIO_OUT_TransferCompleteCallback(void)
{
    if (bsp_audio_out_echo_stream)
    {
        if ((bsp_audio_out_echo_writable & 0x02U) != 0U)
        {
            ++bsp_audio_out_echo_underruns;
        }
        bsp_audio_out_echo_writable |= 0x02U;
        if (bsp_audio_out_signal != NULL)
        {
            bsp_audio_out_signal();
        }
        return;
    }
    __HAL_I2S_DISABLE(&hi2s1);
    bsp_audio_out_busy = false;
    if (!bsp_audio_out_keep_enabled)
    {
        HAL_GPIO_WritePin(I2S_SDMODE_GPIO_Port, I2S_SDMODE_Pin, GPIO_PIN_RESET);
    }
    if (bsp_audio_out_signal != NULL)
    {
        bsp_audio_out_signal();
    }
}

void BSP_AUDIO_OUT_HalfTransferCallback(void)
{
    if (bsp_audio_out_echo_stream)
    {
        if ((bsp_audio_out_echo_writable & 0x01U) != 0U)
        {
            ++bsp_audio_out_echo_underruns;
        }
        bsp_audio_out_echo_writable |= 0x01U;
        if (bsp_audio_out_signal != NULL)
        {
            bsp_audio_out_signal();
        }
    }
}

void BSP_AUDIO_OUT_ErrorCallback(uint32_t hal_error)
{
    __HAL_I2S_DISABLE(&hi2s1);
    bsp_audio_out_last_hal_error = hal_error;
    bsp_audio_out_dma_error = true;
    bsp_audio_out_busy = false;
    HAL_GPIO_WritePin(I2S_SDMODE_GPIO_Port, I2S_SDMODE_Pin, GPIO_PIN_RESET);
    if (bsp_audio_out_signal != NULL)
    {
        bsp_audio_out_signal();
    }
}
