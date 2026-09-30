#include "bsp_lcd.h"

#include "main.h"
#include "st7701s.h"

#define BSP_LCD_PIN_LOW               0U
#define BSP_LCD_PIN_HIGH              1U
#define BSP_LCD_SERIAL_HALF_PERIOD_US 1U
#define BSP_LCD_BACKLIGHT_ON          GPIO_PIN_SET
#define BSP_LCD_BACKLIGHT_OFF         GPIO_PIN_RESET

static ST7701S_Object_t lcd_controller;
static bool lcd_initialized;
static volatile bool lcd_backlight_enabled;
static volatile uint8_t lcd_brightness = 100U;
static uint8_t lcd_pwm_phase;
static uint8_t lcd_pwm_duty;
static volatile uint32_t lcd_serial_clock_edges;
static volatile uint32_t lcd_command_count;

static int32_t BSP_LCD_WriteGPIO(GPIO_TypeDef *port,
                                 uint16_t pin,
                                 uint8_t level)
{
    HAL_GPIO_WritePin(port,
                      pin,
                      (level != BSP_LCD_PIN_LOW) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    return 0;
}

static int32_t BSP_LCD_IO_WriteCS(void *context, uint8_t level)
{
    (void)context;
    if (level == BSP_LCD_PIN_LOW)
    {
        ++lcd_command_count;
    }
    return BSP_LCD_WriteGPIO(LCD_CS_GPIO_Port, LCD_CS_Pin, level);
}

static int32_t BSP_LCD_IO_WriteSCL(void *context, uint8_t level)
{
    (void)context;
    if (level != BSP_LCD_PIN_LOW)
    {
        ++lcd_serial_clock_edges;
    }
    return BSP_LCD_WriteGPIO(LCD_SCL_GPIO_Port, LCD_SCL_Pin, level);
}

static int32_t BSP_LCD_IO_WriteSDA(void *context, uint8_t level)
{
    (void)context;
    return BSP_LCD_WriteGPIO(LCD_SDA_GPIO_Port, LCD_SDA_Pin, level);
}

static int32_t BSP_LCD_IO_WriteReset(void *context, uint8_t level)
{
    (void)context;
    return BSP_LCD_WriteGPIO(LCD_RST_GPIO_Port, LCD_RST_Pin, level);
}

static void BSP_LCD_IO_DelayMs(void *context, uint32_t delay_ms)
{
    (void)context;
    HAL_Delay(delay_ms);
}

static void BSP_LCD_EnableCycleCounter(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void BSP_LCD_IO_DelayUs(void *context, uint32_t delay_us)
{
    uint32_t cycles_per_us;
    uint32_t required_cycles;
    uint32_t start_cycles;

    (void)context;

    if (delay_us == 0U)
    {
        return;
    }

    cycles_per_us = SystemCoreClock / 1000000U;
    if (cycles_per_us == 0U)
    {
        cycles_per_us = 1U;
    }

    /* Serial delays are deliberately short, so this multiplication cannot
       approach the 32-bit cycle-counter wrap interval. */
    required_cycles = cycles_per_us * delay_us;
    start_cycles = DWT->CYCCNT;
    while ((uint32_t)(DWT->CYCCNT - start_cycles) < required_cycles)
    {
        __NOP();
    }
}

static BSP_LCD_Status_t BSP_LCD_FromComponentStatus(ST7701S_Status_t status)
{
    switch (status)
    {
        case ST7701S_OK:
            return BSP_LCD_OK;

        case ST7701S_ERROR_INVALID_ARGUMENT:
            return BSP_LCD_ERROR_INVALID_ARGUMENT;

        case ST7701S_ERROR_NOT_INITIALIZED:
            return BSP_LCD_ERROR_NOT_INITIALIZED;

        case ST7701S_ERROR_IO:
        default:
            return BSP_LCD_ERROR_COMPONENT;
    }
}

BSP_LCD_Status_t BSP_LCD_Init(void)
{
    const ST7701S_IO_t io = {
        .context = NULL,
        .write_cs = BSP_LCD_IO_WriteCS,
        .write_scl = BSP_LCD_IO_WriteSCL,
        .write_sda = BSP_LCD_IO_WriteSDA,
        .write_reset = BSP_LCD_IO_WriteReset,
        .delay_ms = BSP_LCD_IO_DelayMs,
        .delay_us = BSP_LCD_IO_DelayUs,
        .serial_half_period_us = BSP_LCD_SERIAL_HALF_PERIOD_US
    };
    ST7701S_Status_t status;

    lcd_initialized = false;
    lcd_backlight_enabled = false;
    lcd_serial_clock_edges = 0U;
    lcd_command_count = 0U;

    /* Safe panel state: controller selected only during transfers, reset
       asserted, serial clock low and backlight disabled. */
    HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, BSP_LCD_BACKLIGHT_OFF);
    HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_SCL_GPIO_Port, LCD_SCL_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_SDA_GPIO_Port, LCD_SDA_Pin, GPIO_PIN_RESET);

    BSP_LCD_EnableCycleCounter();
    status = ST7701S_RegisterIO(&lcd_controller, &io);
    if (status != ST7701S_OK)
    {
        return BSP_LCD_FromComponentStatus(status);
    }

    lcd_initialized = true;
    return BSP_LCD_OK;
}

BSP_LCD_Status_t BSP_LCD_Reset(void)
{
    if (!lcd_initialized)
    {
        return BSP_LCD_ERROR_NOT_INITIALIZED;
    }

    return BSP_LCD_FromComponentStatus(ST7701S_HardwareReset(&lcd_controller));
}

BSP_LCD_Status_t BSP_LCD_SetBacklight(bool enabled)
{
    if (!lcd_initialized)
    {
        return BSP_LCD_ERROR_NOT_INITIALIZED;
    }

    /* ISR owns the PWM phase. Disabling must also take effect immediately. */
    lcd_backlight_enabled = enabled;
    if (!enabled)
    { HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, BSP_LCD_BACKLIGHT_OFF); }
    return BSP_LCD_OK;
}

BSP_LCD_Status_t BSP_LCD_SetBrightness(uint8_t percent)
{
    if ((percent < 10U) || (percent > 100U) || ((percent % 10U) != 0U))
    { return BSP_LCD_ERROR_INVALID_ARGUMENT; }
    if (!lcd_initialized) { return BSP_LCD_ERROR_NOT_INITIALIZED; }
    lcd_brightness = percent;
    return BSP_LCD_OK;
}

uint8_t BSP_LCD_GetBrightness(void) { return lcd_brightness; }

void BSP_LCD_BacklightTick1ms(void)
{
    if (!lcd_backlight_enabled) { lcd_pwm_phase = 0U; return; }
    /* REV01 PG1 -> STLD40D EN. Datasheet Rev 7 section 5.3 permits low
       frequency PWM. 10 x 1 ms = 100 Hz, with 10% duty steps. Validate
       flicker/current on this PCB; duty is not calibrated luminance.
       Latch changes at a period boundary to avoid malformed pulses. */
    if (lcd_pwm_phase == 0U) { lcd_pwm_duty = lcd_brightness / 10U; }
    HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin,
                      lcd_pwm_phase < lcd_pwm_duty ? BSP_LCD_BACKLIGHT_ON
                                                  : BSP_LCD_BACKLIGHT_OFF);
    lcd_pwm_phase = (uint8_t)((lcd_pwm_phase + 1U) % 10U);
}

BSP_LCD_Status_t BSP_LCD_WriteCommand(uint8_t command,
                                      const uint8_t *parameters,
                                      size_t parameter_count)
{
    if (!lcd_initialized)
    {
        return BSP_LCD_ERROR_NOT_INITIALIZED;
    }

    return BSP_LCD_FromComponentStatus(
        ST7701S_WriteCommand(&lcd_controller,
                             command,
                             parameters,
                             parameter_count));
}

uint32_t BSP_LCD_GetSerialClockEdgeCount(void)
{
    return lcd_serial_clock_edges;
}

uint32_t BSP_LCD_GetCommandCount(void)
{
    return lcd_command_count;
}

BSP_LCD_Status_t BSP_LCD_InitController(void)
{
    if (!lcd_initialized)
    {
        return BSP_LCD_ERROR_NOT_INITIALIZED;
    }

    /* Keep the backlight off until a valid RGB framebuffer is configured. */
    (void)BSP_LCD_SetBacklight(false);
    return BSP_LCD_FromComponentStatus(
        ST7701S_InitDWIN_LI48480T028BA3098(&lcd_controller));
}
