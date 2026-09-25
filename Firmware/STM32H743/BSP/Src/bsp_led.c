#include "bsp_led.h"

#include "tim.h"
#include "ws2812c.h"

#define BSP_LED_TIMER_CHANNEL       TIM_CHANNEL_2
#define BSP_LED_TIMER_PERIOD_TICKS  300U
#define BSP_LED_TIMER_CLOCK_HZ      240000000UL
#define BSP_LED_ZERO_HIGH_TICKS     72U  /* 300 ns */
#define BSP_LED_ONE_HIGH_TICKS      204U /* 850 ns; known-good REV01 bring-up */
#define BSP_LED_RESET_SLOTS         240U /* 300 us; datasheet requires >280 us */
#define BSP_LED_PWM_BUFFER_LENGTH   \
    ((BSP_LED_COUNT * WS2812C_BITS_PER_PIXEL) + BSP_LED_RESET_SLOTS)

static WS2812C_Color_t led_pixels[BSP_LED_COUNT];
static WS2812C_Handle_t led_driver;
static volatile bool led_transfer_active;
static bool led_initialized;
volatile BSP_LED_Diagnostics_t bsp_led_diagnostics;

/* DMA1 cannot access DTCM. The linker places this buffer in D2 SRAM and the
   cache line alignment allows safe D-Cache maintenance before transmission. */
__attribute__((section(".dma_buffer"), aligned(32)))
static uint16_t led_pwm_buffer[BSP_LED_PWM_BUFFER_LENGTH];

static void BSP_LED_CaptureDiagnostics(void)
{
    DMA_HandleTypeDef *const dma = htim1.hdma[TIM_DMA_ID_CC2];

    bsp_led_diagnostics.dma_remaining =
        __HAL_DMA_GET_COUNTER(dma);
    bsp_led_diagnostics.dma_control = DMA1_Stream4->CR;
    bsp_led_diagnostics.dma_high_isr = DMA1->HISR;
    bsp_led_diagnostics.dma_state = (uint32_t)dma->State;
    bsp_led_diagnostics.dma_error = dma->ErrorCode;
    bsp_led_diagnostics.tim_counter = TIM1->CNT;
    bsp_led_diagnostics.tim_dma_interrupt_enable = TIM1->DIER;
    bsp_led_diagnostics.tim_capture_compare_enable = TIM1->CCER;
    bsp_led_diagnostics.primask = __get_PRIMASK();
    bsp_led_diagnostics.basepri = __get_BASEPRI();
}

static BSP_LED_Status_t BSP_LED_FromDriverStatus(WS2812C_Status_t status)
{
    if (status == WS2812C_OK)
    {
        return BSP_LED_OK;
    }
    if ((status == WS2812C_INVALID_ARGUMENT) ||
        (status == WS2812C_INDEX_OUT_OF_RANGE))
    {
        return BSP_LED_INVALID_ARGUMENT;
    }
    return BSP_LED_DRIVER_ERROR;
}

BSP_LED_Status_t BSP_LED_Init(void)
{
    const uint32_t timer_clock_hz = HAL_RCC_GetPCLK2Freq() * 2UL;

    led_initialized = false;
    led_transfer_active = false;
    bsp_led_diagnostics = (BSP_LED_Diagnostics_t){0};

    if ((timer_clock_hz != BSP_LED_TIMER_CLOCK_HZ) ||
        (htim1.Init.Prescaler != 0U) ||
        ((htim1.Init.Period + 1U) != BSP_LED_TIMER_PERIOD_TICKS))
    {
        return BSP_LED_TIMER_CONFIG_ERROR;
    }

    if (WS2812C_Init(&led_driver, led_pixels, BSP_LED_COUNT) != WS2812C_OK)
    {
        return BSP_LED_DRIVER_ERROR;
    }

    __HAL_TIM_SET_COMPARE(&htim1, BSP_LED_TIMER_CHANNEL, 0U);
    led_initialized = true;
    return BSP_LED_OK;
}

BSP_LED_Status_t BSP_LED_SetPixel(size_t index,
                                 uint8_t red,
                                 uint8_t green,
                                 uint8_t blue)
{
    if (!led_initialized)
    {
        return BSP_LED_NOT_INITIALIZED;
    }

    return BSP_LED_FromDriverStatus(
        WS2812C_SetPixel(&led_driver, index,
                        (WS2812C_Color_t){red, green, blue}));
}

BSP_LED_Status_t BSP_LED_SetPixelWithIntensity(size_t index,
                                              uint8_t red,
                                              uint8_t green,
                                              uint8_t blue,
                                              uint8_t intensity)
{
    const uint8_t scaled_red =
        (uint8_t)(((uint16_t)red * intensity + 127U) / 255U);
    const uint8_t scaled_green =
        (uint8_t)(((uint16_t)green * intensity + 127U) / 255U);
    const uint8_t scaled_blue =
        (uint8_t)(((uint16_t)blue * intensity + 127U) / 255U);

    return BSP_LED_SetPixel(index, scaled_red, scaled_green, scaled_blue);
}

BSP_LED_Status_t BSP_LED_Fill(uint8_t red, uint8_t green, uint8_t blue)
{
    if (!led_initialized)
    {
        return BSP_LED_NOT_INITIALIZED;
    }

    return BSP_LED_FromDriverStatus(
        WS2812C_Fill(&led_driver, (WS2812C_Color_t){red, green, blue}));
}

BSP_LED_Status_t BSP_LED_Show(void)
{
    size_t transfer_length;
    WS2812C_Status_t driver_status;

    if (!led_initialized)
    {
        return BSP_LED_NOT_INITIALIZED;
    }
    if (led_transfer_active)
    {
        return BSP_LED_BUSY;
    }

    driver_status = WS2812C_EncodePwm(
        &led_driver, BSP_LED_ZERO_HIGH_TICKS, BSP_LED_ONE_HIGH_TICKS,
        BSP_LED_RESET_SLOTS, led_pwm_buffer, BSP_LED_PWM_BUFFER_LENGTH,
        &transfer_length);
    if (driver_status != WS2812C_OK)
    {
        return BSP_LED_DRIVER_ERROR;
    }

    SCB_CleanDCache_by_Addr((uint32_t *)(void *)led_pwm_buffer,
                            (int32_t)sizeof(led_pwm_buffer));
    led_transfer_active = true;
    ++bsp_led_diagnostics.show_count;
    if (HAL_TIM_PWM_Start_DMA(&htim1, BSP_LED_TIMER_CHANNEL,
                              (const uint32_t *)(const void *)led_pwm_buffer,
                              (uint16_t)transfer_length) != HAL_OK)
    {
        led_transfer_active = false;
        return BSP_LED_HAL_ERROR;
    }

    return BSP_LED_OK;
}

BSP_LED_Status_t BSP_LED_ShowBlocking(uint32_t timeout_ms)
{
    const uint32_t start_tick = HAL_GetTick();
    BSP_LED_Status_t status = BSP_LED_Show();

    if (status != BSP_LED_OK)
    {
        return status;
    }

    while (led_transfer_active)
    {
        if ((HAL_GetTick() - start_tick) >= timeout_ms)
        {
            BSP_LED_CaptureDiagnostics();
            (void)HAL_TIM_PWM_Stop_DMA(&htim1, BSP_LED_TIMER_CHANNEL);
            __HAL_TIM_SET_COMPARE(&htim1, BSP_LED_TIMER_CHANNEL, 0U);
            led_transfer_active = false;
            return BSP_LED_TIMEOUT;
        }
    }

    return BSP_LED_OK;
}

bool BSP_LED_IsBusy(void)
{
    return led_transfer_active;
}

void BSP_LED_TIM_PWM_PulseFinishedCallback(void *timer_instance)
{
    if ((timer_instance == (void *)TIM1) && led_transfer_active)
    {
        ++bsp_led_diagnostics.callback_count;
        BSP_LED_CaptureDiagnostics();
        (void)HAL_TIM_PWM_Stop_DMA(&htim1, BSP_LED_TIMER_CHANNEL);
        __HAL_TIM_SET_COMPARE(&htim1, BSP_LED_TIMER_CHANNEL, 0U);
        led_transfer_active = false;
    }
}
