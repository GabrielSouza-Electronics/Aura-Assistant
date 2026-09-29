#include "bsp_audio_out.h"

#include "i2s.h"
#include "main.h"

#define BSP_AUDIO_OUT_SAMPLE_RATE_HZ 48000U
#define BSP_AUDIO_OUT_MAX_DURATION_MS 500U
#define BSP_AUDIO_OUT_PCM_FADE_SAMPLES 960U
#define BSP_AUDIO_OUT_STEREO_SAMPLES \
    ((BSP_AUDIO_OUT_SAMPLE_RATE_HZ * BSP_AUDIO_OUT_MAX_DURATION_MS * 2U) / 1000U)
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
static volatile bool bsp_audio_out_pcm_stream;
static volatile bool bsp_audio_out_effect;
static volatile uint8_t bsp_audio_out_pcm_end_half;
static volatile uint8_t bsp_audio_out_volume = 5U;
static const int16_t *bsp_audio_out_pcm_source;
static size_t bsp_audio_out_pcm_sample_count;
static size_t bsp_audio_out_pcm_offset;
static volatile uint8_t bsp_audio_out_echo_writable;
static volatile uint32_t bsp_audio_out_echo_underruns;
static BSP_AUDIO_OUT_PrepareWait_t bsp_audio_out_prepare_wait;
static BSP_AUDIO_OUT_Wait_t bsp_audio_out_wait;
static BSP_AUDIO_OUT_Signal_t bsp_audio_out_signal;

void BSP_AUDIO_OUT_SetVolume(uint8_t volume)
{
    bsp_audio_out_volume = (volume > BSP_AUDIO_OUT_MAX_VOLUME)
                               ? BSP_AUDIO_OUT_MAX_VOLUME
                               : volume;
}

uint8_t BSP_AUDIO_OUT_GetVolume(void)
{
    return bsp_audio_out_volume;
}

static void BSP_AUDIO_OUT_FillPCM48kMonoHalf(size_t half)
{
    const size_t half_samples = BSP_AUDIO_OUT_STEREO_SAMPLES / 2U;
    const size_t half_mono_samples = half_samples / 2U;
    const size_t output_offset = half * half_samples;
     const size_t fade_start = (bsp_audio_out_pcm_sample_count >
                                         BSP_AUDIO_OUT_PCM_FADE_SAMPLES)
                                             ? (bsp_audio_out_pcm_sample_count -
                                                 BSP_AUDIO_OUT_PCM_FADE_SAMPLES)
                                             : 0U;
    size_t remaining = bsp_audio_out_pcm_sample_count -
                       bsp_audio_out_pcm_offset;
    const size_t sample_count = (remaining < half_mono_samples)
                                    ? remaining
                                    : half_mono_samples;

    for (size_t index = 0U; index < sample_count; ++index)
    {
        const size_t source_index = bsp_audio_out_pcm_offset + index;
        int32_t sample;

        sample = ((int32_t)bsp_audio_out_pcm_source[source_index] *
              (int32_t)bsp_audio_out_volume) /
             (int32_t)BSP_AUDIO_OUT_MAX_VOLUME;
        if (source_index >= fade_start)
        {
            const size_t fade_length = bsp_audio_out_pcm_sample_count -
                                       fade_start;
            const size_t remaining = bsp_audio_out_pcm_sample_count -
                                     source_index;
            sample = (sample * (int32_t)remaining) /
                     (int32_t)fade_length;
        }

        bsp_audio_out_dma_buffer[output_offset + (2U * index)] = (int16_t)sample;
        bsp_audio_out_dma_buffer[output_offset + (2U * index) + 1U] = (int16_t)sample;
    }
    for (size_t index = sample_count; index < half_mono_samples; ++index)
    {
        bsp_audio_out_dma_buffer[output_offset + (2U * index)] = 0;
        bsp_audio_out_dma_buffer[output_offset + (2U * index) + 1U] = 0;
    }

    bsp_audio_out_pcm_offset += sample_count;
    if (bsp_audio_out_pcm_offset >= bsp_audio_out_pcm_sample_count)
    {
        bsp_audio_out_pcm_end_half = (uint8_t)half;
    }
    SCB_CleanDCache_by_Addr(
        (uint32_t *)&bsp_audio_out_dma_buffer[output_offset],
        (int32_t)(half_samples * sizeof(int16_t)));
}

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
    BSP_AUDIO_OUT_SetVolume(5U);
    bsp_audio_out_busy = false;
    bsp_audio_out_dma_error = false;
    bsp_audio_out_keep_enabled = false;
    bsp_audio_out_last_hal_error = HAL_I2S_ERROR_NONE;
    bsp_audio_out_echo_stream = false;
    bsp_audio_out_pcm_stream = false;
    bsp_audio_out_effect = false;
    bsp_audio_out_pcm_end_half = 0xFFU;
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
        monitor = (monitor * (int32_t)bsp_audio_out_volume) /
                  (int32_t)BSP_AUDIO_OUT_MAX_VOLUME;
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

BSP_AUDIO_OUT_Status_t BSP_AUDIO_OUT_PlayEffect48kMono(
    const int16_t *pcm_mono, size_t sample_count)
{
    if (pcm_mono == NULL || sample_count == 0U ||
        sample_count > BSP_AUDIO_OUT_STEREO_SAMPLES / 2U)
        return BSP_AUDIO_OUT_ERROR_INVALID_ARGUMENT;
    if (bsp_audio_out_busy && !bsp_audio_out_effect)
        return BSP_AUDIO_OUT_ERROR_BUSY;

    /* Stop DMA before touching its buffer; restarting always begins at sample 0. */
    if (HAL_I2S_DMAStop(&hi2s1) != HAL_OK)
    {
        /* HAL_DMA_Abort reports NO_XFER on an already idle stream (e.g. after
         * welcome or a completed effect). DMAStop still disables I2S and
         * restores its READY state. Only that specific idle case is benign. */
        if (HAL_DMA_GetState(hi2s1.hdmatx) != HAL_DMA_STATE_READY ||
            HAL_DMA_GetError(hi2s1.hdmatx) != HAL_DMA_ERROR_NO_XFER)
        {
            bsp_audio_out_last_hal_error = hi2s1.ErrorCode;
            return BSP_AUDIO_OUT_ERROR_DMA;
        }
    }
    bsp_audio_out_busy = false;
    bsp_audio_out_effect = false;
    bsp_audio_out_pcm_stream = false;
    bsp_audio_out_dma_error = false;
    bsp_audio_out_last_hal_error = HAL_I2S_ERROR_NONE;
    if (hi2s1.hdmatx->Init.Mode != DMA_NORMAL)
    {
        (void)HAL_DMA_DeInit(hi2s1.hdmatx);
        hi2s1.hdmatx->Init.Mode = DMA_NORMAL;
        if (HAL_DMA_Init(hi2s1.hdmatx) != HAL_OK)
            return BSP_AUDIO_OUT_ERROR_DMA_START;
    }
    const uint8_t volume = bsp_audio_out_volume;
    for (size_t i = 0U; i < sample_count; ++i)
    {
        const int16_t sample = (int16_t)(((int32_t)pcm_mono[i] * volume) /
                                       (int32_t)BSP_AUDIO_OUT_MAX_VOLUME);
        bsp_audio_out_dma_buffer[2U * i] = sample;
        bsp_audio_out_dma_buffer[2U * i + 1U] = sample;
    }
    SCB_CleanDCache_by_Addr((uint32_t *)bsp_audio_out_dma_buffer,
                           (int32_t)(sample_count * 2U * sizeof(int16_t)));
    HAL_GPIO_WritePin(I2S_SDMODE_GPIO_Port, I2S_SDMODE_Pin, GPIO_PIN_SET);
    bsp_audio_out_effect = true;
    bsp_audio_out_busy = true;
    if (HAL_I2S_Transmit_DMA(&hi2s1, (uint16_t *)(void *)bsp_audio_out_dma_buffer,
                           (uint16_t)(2U * sample_count)) != HAL_OK)
    {
        bsp_audio_out_effect = false;
        bsp_audio_out_busy = false;
        bsp_audio_out_last_hal_error = hi2s1.ErrorCode;
        HAL_GPIO_WritePin(I2S_SDMODE_GPIO_Port, I2S_SDMODE_Pin, GPIO_PIN_RESET);
        return BSP_AUDIO_OUT_ERROR_DMA_START;
    }
    return BSP_AUDIO_OUT_OK;
}

BSP_AUDIO_OUT_Status_t BSP_AUDIO_OUT_PlayPCM48kMonoBlocking(
    const int16_t *pcm_mono, size_t sample_count, uint32_t timeout_ms)
{
    const uint32_t start_tick = HAL_GetTick();

    if ((pcm_mono == NULL) || (sample_count == 0U))
    {
        return BSP_AUDIO_OUT_ERROR_INVALID_ARGUMENT;
    }
    if (bsp_audio_out_busy)
    {
        return BSP_AUDIO_OUT_ERROR_BUSY;
    }

    bsp_audio_out_pcm_source = pcm_mono;
    bsp_audio_out_pcm_sample_count = sample_count;
    bsp_audio_out_pcm_offset = 0U;
    bsp_audio_out_pcm_end_half = 0xFFU;
    bsp_audio_out_pcm_stream = true;
    bsp_audio_out_busy = true;
    bsp_audio_out_dma_error = false;
    bsp_audio_out_keep_enabled = true;
    bsp_audio_out_last_hal_error = HAL_I2S_ERROR_NONE;
    BSP_AUDIO_OUT_FillPCM48kMonoHalf(0U);
    BSP_AUDIO_OUT_FillPCM48kMonoHalf(1U);

    (void)HAL_DMA_DeInit(hi2s1.hdmatx);
    hi2s1.hdmatx->Init.Mode = DMA_CIRCULAR;
    if (HAL_DMA_Init(hi2s1.hdmatx) != HAL_OK)
    {
        bsp_audio_out_pcm_stream = false;
        bsp_audio_out_busy = false;
        bsp_audio_out_last_hal_error = hi2s1.ErrorCode;
        return BSP_AUDIO_OUT_ERROR_DMA_START;
    }

    HAL_GPIO_WritePin(I2S_SDMODE_GPIO_Port, I2S_SDMODE_Pin, GPIO_PIN_SET);
    for (volatile uint32_t delay = 0U; delay < 10000U; ++delay)
    {
        __NOP();
    }
    if (HAL_I2S_Transmit_DMA(&hi2s1,
                             (uint16_t *)(void *)bsp_audio_out_dma_buffer,
                             (uint16_t)BSP_AUDIO_OUT_STEREO_SAMPLES) != HAL_OK)
    {
        bsp_audio_out_pcm_stream = false;
        bsp_audio_out_busy = false;
        bsp_audio_out_last_hal_error = hi2s1.ErrorCode;
        HAL_GPIO_WritePin(I2S_SDMODE_GPIO_Port, I2S_SDMODE_Pin, GPIO_PIN_RESET);
        return BSP_AUDIO_OUT_ERROR_DMA_START;
    }

    while (bsp_audio_out_busy)
    {
        if ((HAL_GetTick() - start_tick) >= timeout_ms)
        {
            (void)HAL_I2S_DMAStop(&hi2s1);
            bsp_audio_out_pcm_stream = false;
            bsp_audio_out_busy = false;
            HAL_GPIO_WritePin(I2S_SDMODE_GPIO_Port, I2S_SDMODE_Pin, GPIO_PIN_RESET);
            return BSP_AUDIO_OUT_ERROR_TIMEOUT;
        }
        if (bsp_audio_out_wait != NULL)
        {
            const uint32_t elapsed = HAL_GetTick() - start_tick;
            const uint32_t remaining = timeout_ms - elapsed;
            if (!bsp_audio_out_wait(remaining) && bsp_audio_out_busy)
            {
                (void)HAL_I2S_DMAStop(&hi2s1);
                bsp_audio_out_pcm_stream = false;
                bsp_audio_out_busy = false;
                HAL_GPIO_WritePin(I2S_SDMODE_GPIO_Port,
                                  I2S_SDMODE_Pin, GPIO_PIN_RESET);
                return BSP_AUDIO_OUT_ERROR_TIMEOUT;
            }
        }
        else
        {
            HAL_Delay(1U);
        }
    }

    return bsp_audio_out_dma_error ? BSP_AUDIO_OUT_ERROR_DMA
                                   : BSP_AUDIO_OUT_OK;
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
    if (bsp_audio_out_effect)
    {
        (void)HAL_I2S_DMAStop(&hi2s1);
        bsp_audio_out_effect = false;
        bsp_audio_out_busy = false;
        HAL_GPIO_WritePin(I2S_SDMODE_GPIO_Port, I2S_SDMODE_Pin, GPIO_PIN_RESET);
        return;
    }
    if (bsp_audio_out_pcm_stream)
    {
        if (bsp_audio_out_pcm_end_half == 1U)
        {
            (void)HAL_I2S_DMAStop(&hi2s1);
            bsp_audio_out_pcm_stream = false;
            bsp_audio_out_busy = false;
            HAL_GPIO_WritePin(I2S_SDMODE_GPIO_Port,
                              I2S_SDMODE_Pin, GPIO_PIN_RESET);
        }
        else
        {
            BSP_AUDIO_OUT_FillPCM48kMonoHalf(1U);
        }
        if (bsp_audio_out_signal != NULL)
        {
            bsp_audio_out_signal();
        }
        return;
    }
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
    if (bsp_audio_out_pcm_stream)
    {
        if (bsp_audio_out_pcm_end_half == 0U)
        {
            (void)HAL_I2S_DMAStop(&hi2s1);
            bsp_audio_out_pcm_stream = false;
            bsp_audio_out_busy = false;
            HAL_GPIO_WritePin(I2S_SDMODE_GPIO_Port,
                              I2S_SDMODE_Pin, GPIO_PIN_RESET);
            if (bsp_audio_out_signal != NULL)
            {
                bsp_audio_out_signal();
            }
            return;
        }
        BSP_AUDIO_OUT_FillPCM48kMonoHalf(0U);
        if (bsp_audio_out_signal != NULL)
        {
            bsp_audio_out_signal();
        }
        return;
    }
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
