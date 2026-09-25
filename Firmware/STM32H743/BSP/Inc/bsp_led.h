#ifndef BSP_LED_H
#define BSP_LED_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BSP_LED_COUNT 10U

typedef enum
{
    BSP_LED_OK = 0,
    BSP_LED_INVALID_ARGUMENT,
    BSP_LED_NOT_INITIALIZED,
    BSP_LED_TIMER_CONFIG_ERROR,
    BSP_LED_DRIVER_ERROR,
    BSP_LED_BUSY,
    BSP_LED_HAL_ERROR,
    BSP_LED_TIMEOUT
} BSP_LED_Status_t;

typedef struct
{
    uint32_t show_count;
    uint32_t callback_count;
    uint32_t dma_remaining;
    uint32_t dma_control;
    uint32_t dma_high_isr;
    uint32_t dma_state;
    uint32_t dma_error;
    uint32_t tim_counter;
    uint32_t tim_dma_interrupt_enable;
    uint32_t tim_capture_compare_enable;
    uint32_t primask;
    uint32_t basepri;
} BSP_LED_Diagnostics_t;

extern volatile BSP_LED_Diagnostics_t bsp_led_diagnostics;

BSP_LED_Status_t BSP_LED_Init(void);
BSP_LED_Status_t BSP_LED_SetPixel(size_t index,
                                 uint8_t red,
                                 uint8_t green,
                                 uint8_t blue);
BSP_LED_Status_t BSP_LED_SetPixelWithIntensity(size_t index,
                                              uint8_t red,
                                              uint8_t green,
                                              uint8_t blue,
                                              uint8_t intensity);
BSP_LED_Status_t BSP_LED_Fill(uint8_t red, uint8_t green, uint8_t blue);
BSP_LED_Status_t BSP_LED_Show(void);
BSP_LED_Status_t BSP_LED_ShowBlocking(uint32_t timeout_ms);
bool BSP_LED_IsBusy(void);

/* Called from the HAL PWM completion callback. */
void BSP_LED_TIM_PWM_PulseFinishedCallback(void *timer_instance);

#endif
