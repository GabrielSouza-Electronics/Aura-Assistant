#include "app.h"

#include "app_hand_tracking.h"
#include "audio_dsp.h"
#include "bsp_flash.h"
#include "bsp_lcd.h"
#include "bsp_led.h"
#include "cmsis_os2.h"
#include "ltdc.h"
#include "welcome_audio.h"

#include <string.h>

#define APP_LCD_WIDTH             480U
#define APP_LCD_HEIGHT            480U
#define APP_LCD_FRAMEBUFFER_WORDS (APP_LCD_WIDTH * APP_LCD_HEIGHT)
#define APP_FLASH_PROBE_BYTES     4096U
#define APP_AUDIO_DMA_DONE_FLAG   (1UL << 0)
#define APP_AUDIO_INPUT_READY_FLAG (1UL << 1)
#define APP_AUDIO_RECORD_RATE_HZ   16000U
#define APP_AUDIO_SETTLE_SECONDS   1U
#define APP_AUDIO_SETTLE_SAMPLES \
    (APP_AUDIO_RECORD_RATE_HZ * APP_AUDIO_SETTLE_SECONDS)
#define APP_AUDIO_RECORD_SECONDS   5U
#define APP_AUDIO_RECORD_SAMPLES \
    (APP_AUDIO_RECORD_RATE_HZ * APP_AUDIO_RECORD_SECONDS)
#define APP_LED_PALETTE_STEPS      768U
#define APP_LED_FRAME_DELAY_MS     35U
#define APP_LED_MIN_INTENSITY      48U
#define APP_LED_ROTATION_STEP      9U
#define APP_LED_BREATH_STEP        2U

typedef struct
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} APP_LED_Color_t;

extern uint8_t __touchgfx_framebuffer_start__;
extern uint8_t __touchgfx_framebuffer_end__;
extern uint8_t __external_flash_start__;
extern uint8_t __external_flash_end__;

static volatile APP_InitStatus_t app_init_status = APP_INIT_NOT_STARTED;
volatile APP_DisplayDiagnostics_t app_display_diagnostics = {
    .magic = 0x41555241U /* "AURA" */
};
volatile BSP_LED_Status_t app_led_status = BSP_LED_NOT_INITIALIZED;
volatile APP_PowerDiagnostics_t app_power_diagnostics = {
    .magic = 0x50575244U /* "PWRD" */
};
volatile APP_IMUDiagnostics_t app_imu_diagnostics = {
    .magic = 0x494D5544U /* "IMUD" */
};
volatile APP_TOFDiagnostics_t app_tof_diagnostics = {
    .magic = 0x544F4644U /* "TOFD" */
};
volatile APP_AudioOutDiagnostics_t app_audio_out_diagnostics = {
    .magic = 0x4155444FU /* "AUDO" */
};
volatile APP_AudioEchoDiagnostics_t app_audio_echo_diagnostics = {
    .magic = 0x4543484FU /* "ECHO" */
};
static osThreadId_t app_audio_output_thread;
static volatile bool app_system_ready;
static volatile bool app_welcome_active;
#if APP_AUDIO_ECHO_TEST
static osThreadId_t app_audio_input_thread;
#endif

__attribute__((section(".dma_buffer.audio_record"), aligned(32)))
static int16_t app_audio_recording[APP_AUDIO_RECORD_SAMPLES];

const int16_t *APP_AudioRecording_Get(size_t *sample_count,
                                      uint32_t *sample_rate_hz)
{
    if (sample_count != NULL)
    {
        *sample_count = app_audio_echo_diagnostics.recorded_samples;
    }
    if (sample_rate_hz != NULL)
    {
        *sample_rate_hz = APP_AUDIO_RECORD_RATE_HZ;
    }
    return app_audio_recording;
}

static void APP_AudioPrepareWait(void)
{
    (void)osThreadFlagsClear(APP_AUDIO_DMA_DONE_FLAG);
}

static bool APP_AudioWait(uint32_t timeout_ms)
{
    const uint32_t flags = osThreadFlagsWait(APP_AUDIO_DMA_DONE_FLAG,
                                              osFlagsWaitAny,
                                              timeout_ms);
    return (flags & osFlagsError) == 0U;
}

static void APP_AudioSignal(void)
{
    if (app_audio_output_thread != NULL)
    {
        (void)osThreadFlagsSet(app_audio_output_thread,
                               APP_AUDIO_DMA_DONE_FLAG);
    }
}

#if APP_AUDIO_ECHO_TEST
static void APP_AudioInputSignal(void)
{
    if (app_audio_input_thread != NULL)
    {
        (void)osThreadFlagsSet(app_audio_input_thread,
                               APP_AUDIO_INPUT_READY_FLAG);
    }
}
#endif

void APP_DisplayDiagnosticsPoll(void)
{
    static uint32_t previous_cpsr;
    const uint32_t current_cpsr = LTDC->CPSR;

    ++app_display_diagnostics.poll_count;
    app_display_diagnostics.init_status = (uint32_t)app_init_status;
    app_display_diagnostics.system_core_clock_hz = SystemCoreClock;
    app_display_diagnostics.ltdc_gcr = LTDC->GCR;
    app_display_diagnostics.ltdc_cpsr = current_cpsr;
    app_display_diagnostics.ltdc_cdsr = LTDC->CDSR;
    app_display_diagnostics.ltdc_isr = LTDC->ISR;
    app_display_diagnostics.ltdc_ier = LTDC->IER;
    app_display_diagnostics.layer_cfb_address = LTDC_Layer1->CFBAR;
    app_display_diagnostics.framebuffer_first_word =
        *(const volatile uint32_t *)(const void *)&__touchgfx_framebuffer_start__;
    app_display_diagnostics.qspi_probe_word =
        *(const volatile uint32_t *)(uintptr_t)0x90000624UL;
    app_display_diagnostics.gpioa_idr = GPIOA->IDR;
    app_display_diagnostics.gpioc_idr = GPIOC->IDR;
    app_display_diagnostics.gpioe_idr = GPIOE->IDR;
    app_display_diagnostics.gpiog_idr = GPIOG->IDR;
    app_display_diagnostics.lcd_serial_clock_edges =
        BSP_LCD_GetSerialClockEdgeCount();
    app_display_diagnostics.lcd_command_count = BSP_LCD_GetCommandCount();
    if (current_cpsr != previous_cpsr)
    {
        ++app_display_diagnostics.line_change_count;
        previous_cpsr = current_cpsr;
    }
}

void HAL_LTDC_ErrorCallback(LTDC_HandleTypeDef *handle)
{
    ++app_display_diagnostics.ltdc_error_count;
    app_display_diagnostics.ltdc_last_error = handle->ErrorCode;
}

static bool APP_FLASH_ValidateMappedContent(void)
{
    const volatile uint8_t *start =
        (const volatile uint8_t *)(const void *)&__external_flash_start__;
    const uintptr_t start_address = (uintptr_t)&__external_flash_start__;
    const uintptr_t end_address = (uintptr_t)&__external_flash_end__;
    size_t probe_length;
    bool found_nonzero = false;
    bool found_non_ff = false;

    if ((end_address <= start_address) ||
        ((end_address - start_address) > (16U * 1024U * 1024U)))
    {
        return false;
    }

    probe_length = end_address - start_address;
    if (probe_length > APP_FLASH_PROBE_BYTES)
    {
        probe_length = APP_FLASH_PROBE_BYTES;
    }

    for (size_t index = 0U; index < probe_length; ++index)
    {
        const uint8_t value = start[index];
        found_nonzero = found_nonzero || (value != 0x00U);
        found_non_ff = found_non_ff || (value != 0xFFU);
    }

    return found_nonzero && found_non_ff;
}

static bool APP_LCD_ClearFramebuffer(void)
{
    volatile uint16_t *framebuffer =
        (volatile uint16_t *)(void *)&__touchgfx_framebuffer_start__;
    const uintptr_t framebuffer_start =
        (uintptr_t)&__touchgfx_framebuffer_start__;
    const uintptr_t framebuffer_end =
        (uintptr_t)&__touchgfx_framebuffer_end__;
    size_t index;

    if ((framebuffer_end < framebuffer_start) ||
        ((framebuffer_end - framebuffer_start) <
         (APP_LCD_FRAMEBUFFER_WORDS * sizeof(uint16_t))))
    {
        return false;
    }

    for (index = 0U; index < APP_LCD_FRAMEBUFFER_WORDS; ++index)
    {
        framebuffer[index] = 0x0000U;
    }

    if (HAL_LTDC_SetAddress(&hltdc, (uint32_t)framebuffer_start, 0U) != HAL_OK)
    {
        return false;
    }

    return true;
}

void APP_Init(void)
{
    BSP_FLASH_Status_t flash_status;
    BSP_LCD_Status_t lcd_status;

    app_init_status = APP_INIT_FLASH_BSP_ERROR;
    flash_status = BSP_FLASH_Init();
    if (flash_status != BSP_FLASH_OK)
    {
        return;
    }

    app_init_status = APP_INIT_FLASH_MEMORY_MAPPED_ERROR;
    flash_status = BSP_FLASH_EnableMemoryMappedMode();
    if (flash_status != BSP_FLASH_OK)
    {
        return;
    }

    app_init_status = APP_INIT_FLASH_CONTENT_ERROR;
    (void)APP_FLASH_ValidateMappedContent();

    app_init_status = APP_INIT_LCD_BSP_ERROR;
    lcd_status = BSP_LCD_Init();
    if (lcd_status != BSP_LCD_OK)
    {
        return;
    }

    app_init_status = APP_INIT_LCD_CONTROLLER_ERROR;
    lcd_status = BSP_LCD_InitController();
    if (lcd_status != BSP_LCD_OK)
    {
        return;
    }

    app_init_status = APP_INIT_LCD_FRAMEBUFFER_ERROR;
    if (!APP_LCD_ClearFramebuffer())
    {
        return;
    }

    app_init_status = APP_INIT_LCD_BACKLIGHT_ERROR;
    lcd_status = BSP_LCD_SetBacklight(true);
    if (lcd_status != BSP_LCD_OK)
    {
        return;
    }

    app_init_status = APP_INIT_OK;
    app_system_ready = true;
    APP_DisplayDiagnosticsPoll();
}

APP_InitStatus_t APP_GetInitStatus(void)
{
    return app_init_status;
}

static uint8_t APP_LED_Interpolate(uint8_t start,
                                   uint8_t end,
                                   uint16_t blend)
{
    return (uint8_t)((((uint32_t)start * (255U - blend)) +
                      ((uint32_t)end * blend) + 127U) / 255U);
}

static APP_LED_Color_t APP_LED_GetStartupColor(uint16_t blend)
{
    return (APP_LED_Color_t){
        APP_LED_Interpolate(255U, 0U, blend),
        0U,
        APP_LED_Interpolate(0U, 255U, blend)
    };
}

void APP_LEDTask(void)
{
    uint16_t rotation = 0U;
    uint8_t breath_phase = 0U;
    uint32_t startup_start_tick = 0U;

    app_led_status = BSP_LED_Init();
    for (;;)
    {
        if (app_led_status == BSP_LED_OK)
        {
            const uint16_t triangle = (breath_phase < 128U) ?
                ((uint16_t)breath_phase * 2U) :
                ((uint16_t)(255U - breath_phase) * 2U);
            const uint8_t breathing_intensity = (uint8_t)(
                APP_LED_MIN_INTENSITY +
                ((triangle * (255U - APP_LED_MIN_INTENSITY)) / 255U));
            APP_LED_Color_t startup_color = {0U, 255U, 0U};

            if (!app_system_ready)
            {
                if (startup_start_tick == 0U)
                {
                    startup_start_tick = osKernelGetTickCount();
                }
                const uint32_t elapsed =
                    osKernelGetTickCount() - startup_start_tick;
                const uint16_t blend = (elapsed >= 4000U)
                    ? 255U
                    : (uint16_t)((elapsed * 255U) / 4000U);
                startup_color = APP_LED_GetStartupColor(blend);
            }

            for (size_t index = 0U; index < BSP_LED_COUNT; ++index)
            {
                uint8_t intensity = breathing_intensity;
                if (!app_system_ready)
                {
                    const uint8_t wave_phase = (uint8_t)(
                        breath_phase + (rotation / 3U) +
                        ((index * 256U) / BSP_LED_COUNT));
                    const uint16_t wave = (wave_phase < 128U)
                        ? ((uint16_t)wave_phase * 2U)
                        : ((uint16_t)(255U - wave_phase) * 2U);
                    intensity = (uint8_t)(APP_LED_MIN_INTENSITY +
                        ((wave * (255U - APP_LED_MIN_INTENSITY)) / 255U));
                }

                const APP_LED_Color_t color = app_system_ready
                    ? (APP_LED_Color_t){0U, 255U, 255U}
                    : startup_color;
                app_led_status = BSP_LED_SetPixelWithIntensity(
                    index, color.red, color.green, color.blue, intensity);
                if (app_led_status != BSP_LED_OK)
                {
                    break;
                }
            }

            if (app_led_status == BSP_LED_OK)
            {
                app_led_status = BSP_LED_ShowBlocking(10U);
            }

            rotation = (uint16_t)((rotation + APP_LED_ROTATION_STEP) %
                                  APP_LED_PALETTE_STEPS);
            breath_phase = (uint8_t)(breath_phase + APP_LED_BREATH_STEP);
        }

        osDelay(APP_LED_FRAME_DELAY_MS);
    }
}
void APP_PowerTask(void)
{
    BSP_POWER_Data_t data = {0};

    app_power_diagnostics.status = BSP_POWER_Init();
    if (app_power_diagnostics.status != BSP_POWER_OK)
    {
        ++app_power_diagnostics.error_count;
    }

    for (;;)
    {
        const BSP_POWER_Status_t status = BSP_POWER_Read(&data);

        app_power_diagnostics.status = status;
        if (status == BSP_POWER_OK)
        {
            app_power_diagnostics.data.battery_adc_raw = data.battery_adc_raw;
            app_power_diagnostics.data.battery_adc_mv = data.battery_adc_mv;
            app_power_diagnostics.data.battery_mv = data.battery_mv;
            app_power_diagnostics.data.battery_percent = data.battery_percent;
            app_power_diagnostics.data.charger_pin_high = data.charger_pin_high;
            app_power_diagnostics.data.usb_status_pin_high = data.usb_status_pin_high;
            app_power_diagnostics.data.charging = data.charging;
            app_power_diagnostics.data.usb_connected = data.usb_connected;
            ++app_power_diagnostics.update_count;
        }
        else
        {
            ++app_power_diagnostics.error_count;
        }

        osDelay(250U);
    }
}

void APP_SensorTask(void)
{
    BSP_IMU_Data_t imu_data = {0};
    BSP_TOF_Data_t tof_data = {0};

    app_imu_diagnostics.init_status = BSP_IMU_Init();
    app_imu_diagnostics.read_status = app_imu_diagnostics.init_status;
    if (app_imu_diagnostics.init_status != BSP_IMU_OK)
    {
        ++app_imu_diagnostics.error_count;
    }

    app_tof_diagnostics.init_status = BSP_TOF_Init();
    app_tof_diagnostics.read_status = app_tof_diagnostics.init_status;
    if (app_tof_diagnostics.init_status != BSP_TOF_OK)
    {
        ++app_tof_diagnostics.error_count;
    }

    for (;;)
    {
        if (app_imu_diagnostics.init_status == BSP_IMU_OK)
        {
            const BSP_IMU_Status_t imu_status = BSP_IMU_Read(&imu_data);
            app_imu_diagnostics.read_status = imu_status;
            if (imu_status == BSP_IMU_OK)
            {
                app_imu_diagnostics.data = imu_data;
                ++app_imu_diagnostics.update_count;
            }
            else
            {
                ++app_imu_diagnostics.error_count;
            }
        }

        if (app_tof_diagnostics.init_status == BSP_TOF_OK)
        {
            const BSP_TOF_Status_t tof_status = BSP_TOF_Read(&tof_data);
            app_tof_diagnostics.read_status = tof_status;
            if (tof_status == BSP_TOF_OK)
            {
                app_tof_diagnostics.data = tof_data;
                ++app_tof_diagnostics.update_count;
                APP_HandTracking_Process(&tof_data);
            }
            else if (tof_status == BSP_TOF_NO_NEW_DATA)
            {
                ++app_tof_diagnostics.no_data_count;
            }
            else
            {
                ++app_tof_diagnostics.error_count;
            }
        }

        osDelay(20U);
    }
}
void APP_AudioOutputTask(void)
{
#if APP_AUDIO_ECHO_TEST
    /* The input task owns I2S1 while the hardware loopback is enabled. */
    for (;;)
    {
        osDelay(1000U);
    }
#else
    app_audio_output_thread = osThreadGetId();
    BSP_AUDIO_OUT_SetSynchronizationHooks(APP_AudioPrepareWait,
                                           APP_AudioWait,
                                           APP_AudioSignal);
    app_audio_out_diagnostics.init_status = BSP_AUDIO_OUT_Init();
    app_audio_out_diagnostics.last_status =
        app_audio_out_diagnostics.init_status;
    if (app_audio_out_diagnostics.init_status != BSP_AUDIO_OUT_OK)
    {
        ++app_audio_out_diagnostics.error_count;
    }
    else
    {
        while (!app_system_ready)
        {
            osDelay(10U);
        }

        BSP_AUDIO_OUT_SetVolume(5U);
        app_welcome_active = true;
        app_audio_out_diagnostics.busy = true;
        app_audio_out_diagnostics.last_status =
            BSP_AUDIO_OUT_PlayPCM48kMonoBlocking(
                welcome_audio_pcm, welcome_audio_pcm_count, 3000U);
        app_audio_out_diagnostics.busy = false;
        app_welcome_active = false;
        app_audio_out_diagnostics.last_hal_error =
            BSP_AUDIO_OUT_GetLastHALerror();
        if (app_audio_out_diagnostics.last_status == BSP_AUDIO_OUT_OK)
        {
            ++app_audio_out_diagnostics.play_count;
        }
        else
        {
            ++app_audio_out_diagnostics.error_count;
        }
    }
    for (;;)
    {
        osDelay(1000U);
    }
#endif
}

void APP_AudioInputTask(void)
{
#if APP_AUDIO_ECHO_TEST
    static int16_t pcm[BSP_AUDIO_IN_BLOCK_FRAMES * BSP_AUDIO_IN_CHANNELS];
    static int16_t processed[BSP_AUDIO_IN_BLOCK_FRAMES];
    AUDIO_DSP_State_t dsp;
    AUDIO_DSP_Metrics_t dsp_metrics = {0};
    size_t settle_samples = 0U;
    size_t record_index = 0U;

    app_audio_input_thread = osThreadGetId();
    app_audio_output_thread = app_audio_input_thread;
    BSP_AUDIO_OUT_SetSynchronizationHooks(APP_AudioPrepareWait,
                                           APP_AudioWait,
                                           APP_AudioSignal);
    app_audio_out_diagnostics.init_status = BSP_AUDIO_OUT_Init();
    app_audio_echo_diagnostics.init_status = BSP_AUDIO_IN_Init();
    AUDIO_DSP_Init(&dsp);
    app_audio_echo_diagnostics.start_status =
        BSP_AUDIO_IN_Start(APP_AudioInputSignal);
    app_audio_echo_diagnostics.running =
        (app_audio_out_diagnostics.init_status == BSP_AUDIO_OUT_OK) &&
        (app_audio_echo_diagnostics.init_status == BSP_AUDIO_IN_OK) &&
        (app_audio_echo_diagnostics.start_status == BSP_AUDIO_IN_OK);
    app_audio_echo_diagnostics.recording =
        app_audio_echo_diagnostics.running;
    app_audio_echo_diagnostics.playing = false;
    memset(app_audio_recording, 0, sizeof(app_audio_recording));

    if (!app_audio_echo_diagnostics.running)
    {
        ++app_audio_echo_diagnostics.error_count;
    }

    while (app_audio_echo_diagnostics.recording)
    {
        (void)osThreadFlagsWait(APP_AUDIO_INPUT_READY_FLAG,
                                osFlagsWaitAny, osWaitForever);
        for (;;)
        {
            size_t frames = 0U;
            const BSP_AUDIO_IN_Status_t input_status =
                BSP_AUDIO_IN_ProcessNextBlock(pcm, BSP_AUDIO_IN_BLOCK_FRAMES,
                                               &frames);
            app_audio_echo_diagnostics.last_input_status = input_status;
            if (input_status == BSP_AUDIO_IN_NO_DATA)
            {
                break;
            }
            if ((input_status != BSP_AUDIO_IN_OK) || (frames == 0U))
            {
                ++app_audio_echo_diagnostics.error_count;
                break;
            }

            uint32_t peak_left = 0U;
            uint32_t peak_right = 0U;
            for (size_t frame = 0U; frame < frames; ++frame)
            {
                const int32_t left = pcm[2U * frame];
                const int32_t right = pcm[(2U * frame) + 1U];
                const uint32_t abs_left = (uint32_t)((left < 0) ? -left : left);
                const uint32_t abs_right = (uint32_t)((right < 0) ? -right : right);
                if (abs_left > peak_left) { peak_left = abs_left; }
                if (abs_right > peak_right) { peak_right = abs_right; }
            }
            app_audio_echo_diagnostics.peak_left = peak_left;
            app_audio_echo_diagnostics.peak_right = peak_right;

            const size_t processed_frames = AUDIO_DSP_ProcessStereo16(
                &dsp, pcm, frames, processed, &dsp_metrics);
            if (settle_samples < APP_AUDIO_SETTLE_SAMPLES)
            {
                const size_t remaining =
                    APP_AUDIO_SETTLE_SAMPLES - settle_samples;
                const size_t discarded =
                    (processed_frames < remaining) ? processed_frames : remaining;
                settle_samples += discarded;

                /* The SAI/PDM stream and decimators remain running during the
                   pre-roll. Reset only the digital conditioning state after
                   the startup transient has passed, so it cannot bias the AGC
                   or leak into the five-second recording. */
                if (settle_samples >= APP_AUDIO_SETTLE_SAMPLES)
                {
                    AUDIO_DSP_Init(&dsp);
                }
            }
            else
            {
                for (size_t frame = 0U;
                     (frame < processed_frames) &&
                     (record_index < APP_AUDIO_RECORD_SAMPLES);
                     ++frame)
                {
                    app_audio_recording[record_index++] = processed[frame];
                }
            }
            app_audio_echo_diagnostics.recorded_samples = record_index;
            app_audio_echo_diagnostics.processed_peak =
                dsp_metrics.output_peak;
            app_audio_echo_diagnostics.combined_peak =
                dsp_metrics.input_peak;
            app_audio_echo_diagnostics.noise_floor = dsp_metrics.noise_floor;
            app_audio_echo_diagnostics.gate_threshold =
                dsp_metrics.gate_threshold;
            app_audio_echo_diagnostics.gain_q12 = dsp_metrics.gain_q12;
            app_audio_echo_diagnostics.right_polarity =
                dsp_metrics.right_polarity;
            app_audio_echo_diagnostics.speech_detected =
                dsp_metrics.speech_detected != 0U;
            ++app_audio_echo_diagnostics.block_count;
            app_audio_echo_diagnostics.overrun_count =
                BSP_AUDIO_IN_GetOverrunCount();
            app_audio_echo_diagnostics.output_underrun_count =
                BSP_AUDIO_OUT_GetEchoUnderrunCount();
            app_audio_echo_diagnostics.input_hal_error =
                BSP_AUDIO_IN_GetLastHALerror();
            app_audio_echo_diagnostics.output_hal_error =
                BSP_AUDIO_OUT_GetLastHALerror();

            if (record_index >= APP_AUDIO_RECORD_SAMPLES)
            {
                app_audio_echo_diagnostics.recording = false;
                break;
            }
        }
    }

    (void)BSP_AUDIO_IN_Stop();
    /* ST-LINK and future DMA/network consumers read physical D2 SRAM rather
       than the Cortex-M7 cache. Publish the completed PCM buffer explicitly. */
    SCB_CleanDCache_by_Addr((uint32_t *)(void *)app_audio_recording,
                           sizeof(app_audio_recording));
    app_audio_echo_diagnostics.last_output_status =
        BSP_AUDIO_OUT_StartEchoStream();
    app_audio_echo_diagnostics.playing =
        (app_audio_echo_diagnostics.last_output_status == BSP_AUDIO_OUT_OK);
    if (!app_audio_echo_diagnostics.playing)
    {
        ++app_audio_echo_diagnostics.error_count;
    }

    size_t playback_index = 0U;
    while (app_audio_echo_diagnostics.playing &&
           (playback_index < APP_AUDIO_RECORD_SAMPLES))
    {
        for (size_t frame = 0U; frame < BSP_AUDIO_IN_BLOCK_FRAMES; ++frame)
        {
            int32_t sample = app_audio_recording[playback_index + frame];
            const size_t remaining =
                APP_AUDIO_RECORD_SAMPLES - (playback_index + frame);
            if (remaining < 1024U)
            {
                sample = (sample * (int32_t)remaining) / 1024;
            }
            pcm[2U * frame] = sample;
            pcm[(2U * frame) + 1U] = sample;
        }

        const BSP_AUDIO_OUT_Status_t status =
            BSP_AUDIO_OUT_QueueEchoBlock(pcm, BSP_AUDIO_IN_BLOCK_FRAMES);
        app_audio_echo_diagnostics.last_output_status = status;
        if (status == BSP_AUDIO_OUT_OK)
        {
            playback_index += BSP_AUDIO_IN_BLOCK_FRAMES;
        }
        else if (status != BSP_AUDIO_OUT_ERROR_BUSY)
        {
            ++app_audio_echo_diagnostics.error_count;
            app_audio_echo_diagnostics.playing = false;
            break;
        }
        else
        {
            (void)osThreadFlagsWait(APP_AUDIO_DMA_DONE_FLAG,
                                    osFlagsWaitAny, 50U);
        }
        app_audio_echo_diagnostics.output_underrun_count =
            BSP_AUDIO_OUT_GetEchoUnderrunCount();
    }

    /* Ramp-down above avoids a discontinuity. Queue silence through both DMA
       halves so stale audio cannot repeat before the amplifier is stopped. */
    memset(pcm, 0, sizeof(pcm));
    for (uint32_t silence_block = 0U; silence_block < 3U; ++silence_block)
    {
        BSP_AUDIO_OUT_Status_t status;
        do
        {
            status = BSP_AUDIO_OUT_QueueEchoBlock(
                pcm, BSP_AUDIO_IN_BLOCK_FRAMES);
            if (status == BSP_AUDIO_OUT_ERROR_BUSY)
            {
                (void)osThreadFlagsWait(APP_AUDIO_DMA_DONE_FLAG,
                                        osFlagsWaitAny, 50U);
            }
        } while (status == BSP_AUDIO_OUT_ERROR_BUSY);
        if (status != BSP_AUDIO_OUT_OK)
        {
            ++app_audio_echo_diagnostics.error_count;
            break;
        }
    }
    osDelay(25U);
    (void)BSP_AUDIO_OUT_StopEchoStream();
    app_audio_echo_diagnostics.playing = false;
    app_audio_echo_diagnostics.running = false;

    for (;;)
    {
        osDelay(1000U);
    }
#else
    for (;;)
    {
        osDelay(1000U);
    }
#endif
}

void APP_SystemTask(void)
{
    static uint8_t menu_option = 0U;
    static uint8_t last_menu_option = 0U;
    static int8_t last_x = 0;
    static int8_t last_y = 0;

    APP_Init();

    for (;;)
    {
        if(app_hand_tracking.hand_active==true) // Check if the hand is active
        {
            if((last_x != app_hand_tracking.x) || (last_y != app_hand_tracking.y)) // Check if the hand position has changed
            {
                if(app_hand_tracking.y >= -2) // Up Direction
                {
                    if(app_hand_tracking.x >= 2) // Up Mid Right 
                    {
                        menu_option=2; // Email
                    }

                    else if(app_hand_tracking.x <= -2) // Up Mid Left 
                    {
                        menu_option=6; // Tasks
                    }
                    else // Up Mid Center
                    {
                        menu_option=1; // Calendar
                    }
                }
                else if(app_hand_tracking.y <= -5) // Down Direction
                {
                    if(app_hand_tracking.x >= 2) // Down Mid Right 
                    {
                        menu_option=3; // Assistant
                    }

                    else if(app_hand_tracking.x <= -2) // Down Mid Left 
                    {
                        menu_option=4; // Reminders
                    }
                    else // Down Mid Center
                    {
                        menu_option=0; // Idle
                    } 
                }
                else
                {
                    menu_option=0; // Idle
                }
                            
                if(last_menu_option != menu_option)
                {
                    last_menu_option = menu_option;
                }            
                last_x = app_hand_tracking.x;
                last_y = app_hand_tracking.y;
            }
        }
        else
        {
            if(menu_option != 0)
            {
                menu_option=0; // Idle
            }
        }

        osDelay(1U);
    }
}
