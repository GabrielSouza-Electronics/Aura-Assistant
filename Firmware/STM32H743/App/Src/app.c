#include "app.h"

#include "app_hand_tracking.h"
#include "app_ui_audio.h"
#include "bsp_audio_out.h"
#include "bsp_lcd.h"
#include "bsp_led.h"
#include "bsp_tof.h"
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "ltdc.h"
#include "task.h"
#include "welcome_audio.h"

#include <string.h>

#define APP_AUDIO_DMA_DONE_FLAG    (1UL << 0)
#define APP_AUDIO_DEFAULT_VOLUME   5U
#define APP_WELCOME_TIMEOUT_MS     3000U
#define APP_LED_PALETTE_STEPS      768U
#define APP_LED_FRAME_DELAY_MS     35U
#define APP_LED_BREATH_DELAY_MS    10U
#define APP_LED_BREATH_SMOOTHING   8
#define APP_LED_MIN_INTENSITY      48U
#define APP_LED_ROTATION_STEP      9U
#define APP_LED_BREATH_STEP        2U
#define APP_LED_SHOW_TIMEOUT_MS    10U
#define APP_POWER_PERIOD_MS        1000U
#define APP_TOF_POLL_PERIOD_MS     20U
#define APP_TOF_STARTUP_FRAMES     3U

typedef struct
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} APP_LED_Color_t;

/* Linker-defined; the linker script asserts exactly 480x480 RGB565. */
extern uint8_t __touchgfx_framebuffer_start__;
extern uint8_t __touchgfx_framebuffer_end__;

static volatile APP_InitStatus_t app_init_status = APP_INIT_NOT_STARTED;
static osThreadId_t app_audio_output_thread;
static volatile bool app_system_ready;
static bool app_display_backlight_on;
static bool app_display_startup_complete;
static volatile bool app_tof_ready;
static volatile uint8_t app_hero_breath;
/* Single aligned publication keeps phase and visibility from the same UI tick. */
static volatile uint32_t app_led_carousel;
static volatile uint32_t app_led_menu_enter_sequence;
/* 0x00RRGGBB, written as one word by the UI task. Default: cyan. */
static volatile uint32_t app_led_breath_color = 0x0000FFFFUL;
static BSP_POWER_Data_t app_power_data;
static volatile bool app_power_valid;

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

static bool APP_LCD_ClearFramebuffer(void)
{
    uint8_t *const framebuffer = &__touchgfx_framebuffer_start__;

    (void)memset(framebuffer, 0,
                 (size_t)(&__touchgfx_framebuffer_end__ - framebuffer));
    return HAL_LTDC_SetAddress(&hltdc, (uint32_t)(uintptr_t)framebuffer,
                               0U) == HAL_OK;
}

void APP_Init(void)
{
    BSP_LCD_Status_t lcd_status;

    /* QSPI flash is already initialized and memory-mapped by
       MX_QUADSPI_Init(), which traps in Error_Handler() on failure. */
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

    /* LCD/assets are ready: allow the splash while the sensor starts ranging. */
    app_init_status = APP_INIT_WAITING_FOR_TOF;
    while (!app_tof_ready)
    {
        osDelay(10U);
    }

    app_init_status = APP_INIT_OK;
}

void APP_DisplayStartupComplete(void)
{
    /* UI task requests release after laying out the complete home screen. */
    app_display_startup_complete = true;
}

void APP_DisplayFramePresented(void)
{
    if (!app_display_backlight_on &&
        ((app_init_status == APP_INIT_WAITING_FOR_TOF) ||
         (app_init_status == APP_INIT_OK)))
    {
        if (BSP_LCD_SetBacklight(true) != BSP_LCD_OK)
        {
            app_init_status = APP_INIT_LCD_BACKLIGHT_ERROR;
            return;
        }
        app_display_backlight_on = true;
    }
    if (app_display_backlight_on && app_display_startup_complete &&
        (app_init_status == APP_INIT_OK))
    {
        /* Release sound/LEDs only after the full home screen is drawn. */
        app_system_ready = true;
    }
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

static APP_LED_Color_t APP_LED_GetStartupColor(uint32_t elapsed_ticks)
{
    static const APP_LED_Color_t palette[] = {
        {255U,  32U,  64U}, /* Coral */
        {255U,  96U,   0U}, /* Orange */
        {255U, 220U,   0U}, /* Yellow */
        { 48U, 255U,  32U}, /* Lime */
        {  0U, 255U, 160U}, /* Mint */
        {  0U, 160U, 255U}, /* Sky blue */
        {160U,  32U, 255U}, /* Purple */
        {255U,  16U, 144U}  /* Pink */
    };
    const uint32_t color_count = sizeof(palette) / sizeof(palette[0]);
    const uint32_t transition_ms = 1200U;
    const uint32_t cycle_ms = (uint32_t)(
        (((uint64_t)elapsed_ticks * 1000U) / osKernelGetTickFreq()) %
        (color_count * transition_ms));
    const uint32_t current = cycle_ms / transition_ms;
    const uint32_t next = (current + 1U) % color_count;
    const uint16_t blend = (uint16_t)(
        ((cycle_ms % transition_ms) * 255U) / transition_ms);

    /* Wrap pink back into coral with the same blend as every other segment. */
    return (APP_LED_Color_t){
        APP_LED_Interpolate(palette[current].red, palette[next].red, blend),
        APP_LED_Interpolate(palette[current].green, palette[next].green, blend),
        APP_LED_Interpolate(palette[current].blue, palette[next].blue, blend)
    };
}

void APP_LED_SetHeroBreath(uint8_t level)
{
    app_hero_breath = level;
}

void APP_LED_SetCarousel(uint16_t phase, uint8_t visibility)
{
    app_led_carousel = (uint32_t)phase | ((uint32_t)visibility << 16);
}

void APP_LED_MenuEnterPulse(void)
{
    ++app_led_menu_enter_sequence;
}

void APP_LED_SetBreathColor(uint8_t red, uint8_t green, uint8_t blue)
{
    app_led_breath_color = ((uint32_t)red << 16) | ((uint32_t)green << 8) | blue;
}

void APP_LEDTask(void)
{
    uint16_t rotation = 0U;
    uint8_t breath_phase = 0U;
    int32_t smoothed_intensity_q8 = (int32_t)APP_LED_MIN_INTENSITY * 256;
    int32_t breath_rgb_q8[3] = {0, 255 * 256, 255 * 256};
    const uint32_t startup_start_tick = osKernelGetTickCount();
    uint32_t pulse_sequence = 0U;
    uint32_t pulse_start_tick = 0U;
    bool pulse_active = false;
    BSP_LED_Status_t led_status = BSP_LED_Init();

    for (;;)
    {
        if (led_status == BSP_LED_OK)
        {
            const bool system_ready = app_system_ready;
            const uint32_t requested_pulse = app_led_menu_enter_sequence;
            const uint32_t now = osKernelGetTickCount();
            uint16_t pulse_scale = 255U;
            if (system_ready && (requested_pulse != pulse_sequence))
            {
                pulse_sequence = requested_pulse;
                pulse_start_tick = now;
                pulse_active = true;
            }
            if (pulse_active)
            {
                const uint32_t elapsed_ms = (uint32_t)(
                    ((uint64_t)(now - pulse_start_tick) * 1000U) /
                    osKernelGetTickFreq());
                if (elapsed_ms >= 360U)
                {
                    pulse_active = false;
                }
                else
                {
                    /* Smooth down/up in 360 ms; retain at least 40% brightness. */
                    const uint32_t half_ms = (elapsed_ms <= 180U)
                        ? elapsed_ms : 360U - elapsed_ms;
                    const uint32_t phase = (half_ms * 255U) / 180U;
                    const uint32_t envelope =
                        (phase * phase * (765U - 2U * phase)) / 65025U;
                    pulse_scale = (uint16_t)(255U - (153U * envelope) / 255U);
                }
            }
            const uint32_t carousel = app_led_carousel;
            const uint16_t carousel_phase = (uint16_t)carousel;
            const uint8_t carousel_visibility = (uint8_t)(carousel >> 16);
            /* Physical LED order runs opposite to the carousel's angle. */
            const uint16_t reverse_phase = (uint16_t)(0U - carousel_phase);
            const uint8_t hero_breath = app_hero_breath;
            const int32_t target_intensity_q8 =
                (int32_t)APP_LED_MIN_INTENSITY * 256 +
                (int32_t)(((uint32_t)hero_breath *
                           (255U - APP_LED_MIN_INTENSITY) * 256U) / 255U);
            if (system_ready)
            {
                /* Smooth the Hero's discrete frames at ~100 Hz. Keep fractional
                   brightness so small changes are not lost to integer rounding.
                   A 1/8 step adds about 75 ms of smoothing without overshoot. */
                smoothed_intensity_q8 +=
                    (target_intensity_q8 - smoothed_intensity_q8) /
                    APP_LED_BREATH_SMOOTHING;

                /* Same smoothing for menu hue changes (~80 ms crossfade). */
                const uint32_t target_rgb = app_led_breath_color;
                for (uint32_t channel = 0U; channel < 3U; ++channel)
                {
                    const int32_t target_q8 =
                        (int32_t)((target_rgb >> (16U - 8U * channel)) & 0xFFU) * 256;
                    breath_rgb_q8[channel] +=
                        (target_q8 - breath_rgb_q8[channel]) / APP_LED_BREATH_SMOOTHING;
                }
            }
            const uint8_t breathing_intensity =
                (uint8_t)((smoothed_intensity_q8 + 128) / 256);
            APP_LED_Color_t startup_color = {0U, 255U, 0U};

            if (!system_ready)
            {
                const uint32_t elapsed =
                    osKernelGetTickCount() - startup_start_tick;
                startup_color = APP_LED_GetStartupColor(elapsed);
            }

            for (size_t index = 0U; index < BSP_LED_COUNT; ++index)
            {
                uint8_t intensity = breathing_intensity;
                if (!system_ready)
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

                APP_LED_Color_t color = system_ready
                    ? (APP_LED_Color_t){(uint8_t)((breath_rgb_q8[0] + 128) / 256),
                                        (uint8_t)((breath_rgb_q8[1] + 128) / 256),
                                        (uint8_t)((breath_rgb_q8[2] + 128) / 256)}
                    : startup_color;
                if (system_ready && (carousel_visibility != 0U))
                {
                    /* Two broad waves circulate with the actual carousel angle.
                       Fractional LED positions crossfade instead of snapping to
                       an index. Smoothstep flattens both bright and dark peaks. */
                    const uint16_t pixel_phase =
                        (uint16_t)((index * 65536U) / BSP_LED_COUNT);
                    const uint16_t wave_phase = (uint16_t)(
                        2U * (uint16_t)(pixel_phase - reverse_phase));
                    const uint32_t triangle = (wave_phase <= 32768U)
                        ? wave_phase : 65536U - wave_phase;
                    const uint32_t ramp = (triangle * 255U) / 32768U;
                    const uint16_t glow = (uint16_t)(
                        (ramp * ramp * (765U - 2U * ramp)) / 65025U);
                    /* Navy troughs, blue shoulders and cyan crests; never off. */
                    const uint8_t wave_green = APP_LED_Interpolate(8U, 255U, glow);
                    const uint8_t wave_blue = APP_LED_Interpolate(64U, 255U, glow);
                    color.green = APP_LED_Interpolate(
                        breathing_intensity, wave_green, carousel_visibility);
                    color.blue = APP_LED_Interpolate(
                        breathing_intensity, wave_blue, carousel_visibility);
                    intensity = 255U;
                }
                if (system_ready)
                {
                    intensity = (uint8_t)(((uint32_t)intensity * pulse_scale + 127U) / 255U);
                }
                led_status = BSP_LED_SetPixelWithIntensity(
                    index, color.red, color.green, color.blue, intensity);
                if (led_status != BSP_LED_OK)
                {
                    break;
                }
            }

            if (led_status == BSP_LED_OK)
            {
                led_status = BSP_LED_ShowBlocking(APP_LED_SHOW_TIMEOUT_MS);
            }

            rotation = (uint16_t)((rotation + APP_LED_ROTATION_STEP) %
                                  APP_LED_PALETTE_STEPS);
            breath_phase = (uint8_t)(breath_phase + APP_LED_BREATH_STEP);
        }

        osDelay(app_system_ready ? APP_LED_BREATH_DELAY_MS : APP_LED_FRAME_DELAY_MS);
    }
}

bool APP_Power_GetData(BSP_POWER_Data_t *data)
{
    bool valid;

    if (data == NULL)
    {
        return false;
    }
    taskENTER_CRITICAL();
    valid = app_power_valid;
    if (valid)
    {
        *data = app_power_data;
    }
    taskEXIT_CRITICAL();
    return valid;
}

void APP_PowerTask(void)
{
    BSP_POWER_Data_t data;

    /* BSP_POWER_Read() retries the ADC calibration if this first attempt fails. */
    (void)BSP_POWER_Init();
    for (;;)
    {
        if (BSP_POWER_Read(&data) == BSP_POWER_OK)
        {
            taskENTER_CRITICAL();
            app_power_data = data;
            app_power_valid = true;
            taskEXIT_CRITICAL();
        }
        osDelay(APP_POWER_PERIOD_MS);
    }
}

void APP_SensorTask(void)
{
    BSP_TOF_Data_t tof_data;
    uint8_t startup_frames = 0U;

    if (BSP_TOF_Init() != BSP_TOF_OK)
    {
        /* Without ranging the UI stays in APP_INIT_WAITING_FOR_TOF. */
        for (;;)
        {
            osDelay(osWaitForever);
        }
    }

    for (;;)
    {
        const BSP_TOF_Status_t status = BSP_TOF_Read(&tof_data);

        if (status == BSP_TOF_OK)
        {
            APP_HandTracking_Process(&tof_data);
            /* A complete new frame proves ranging, even with no target. */
            if (startup_frames < APP_TOF_STARTUP_FRAMES)
            {
                ++startup_frames;
                if (startup_frames == APP_TOF_STARTUP_FRAMES)
                {
                    app_tof_ready = true;
                }
            }
        }
        else if (status != BSP_TOF_NO_NEW_DATA)
        {
            /* NO_NEW_DATA between frames is normal at 5 Hz. */
            startup_frames = 0U;
        }
        osDelay(APP_TOF_POLL_PERIOD_MS);
    }
}

void APP_AudioOutputTask(void)
{
    app_audio_output_thread = osThreadGetId();
    BSP_AUDIO_OUT_SetSynchronizationHooks(APP_AudioPrepareWait,
                                           APP_AudioWait,
                                           APP_AudioSignal);
    if (BSP_AUDIO_OUT_Init() == BSP_AUDIO_OUT_OK)
    {
        while (!app_system_ready)
        {
            osDelay(10U);
        }
        BSP_AUDIO_OUT_SetVolume(APP_AUDIO_DEFAULT_VOLUME);
        (void)BSP_AUDIO_OUT_PlayPCM48kMonoBlocking(welcome_audio_pcm,
                                                   welcome_audio_pcm_count,
                                                   APP_WELCOME_TIMEOUT_MS);
    }
    APP_UIAudio_Run();
}

void APP_AudioInputTask(void)
{
    /* Microphone capture (BSP_AUDIO_IN) is not part of the product flow yet.
       Park the CubeMX-created task without periodic wake-ups. */
    for (;;)
    {
        osDelay(osWaitForever);
    }
}

void APP_SystemTask(void)
{
    APP_Init();

    /* Runtime work is owned by the dedicated tasks; nothing left to poll. */
    for (;;)
    {
        osDelay(osWaitForever);
    }
}
