#ifndef ST7701S_H
#define ST7701S_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/* MIPI DCS commands supported by the ST7701S. */
#define ST7701S_CMD_SOFTWARE_RESET       0x01U
#define ST7701S_CMD_SLEEP_IN             0x10U
#define ST7701S_CMD_SLEEP_OUT            0x11U
#define ST7701S_CMD_DISPLAY_OFF          0x28U
#define ST7701S_CMD_DISPLAY_ON           0x29U
#define ST7701S_CMD_ADDRESS_MODE         0x36U
#define ST7701S_CMD_PIXEL_FORMAT         0x3AU
#define ST7701S_CMD_COMMAND_PAGE_SELECT  0xFFU

#define ST7701S_PIXEL_FORMAT_RGB666      0x66U
#define ST7701S_PIXEL_FORMAT_RGB888      0x77U

typedef enum
{
    ST7701S_OK = 0,
    ST7701S_ERROR_INVALID_ARGUMENT,
    ST7701S_ERROR_IO,
    ST7701S_ERROR_NOT_INITIALIZED
} ST7701S_Status_t;

typedef int32_t (*ST7701S_WritePin_Fn)(void *context, uint8_t level);
typedef void (*ST7701S_DelayMs_Fn)(void *context, uint32_t delay_ms);
typedef void (*ST7701S_DelayUs_Fn)(void *context, uint32_t delay_us);

/**
 * GPIO abstraction used by the 3-line, 9-bit serial transport.
 *
 * The component driver owns the serial protocol. The BSP callbacks own the
 * physical pins and must return zero on success. SDA is only driven as an
 * output by this driver.
 */
typedef struct
{
    void *context;
    ST7701S_WritePin_Fn write_cs;
    ST7701S_WritePin_Fn write_scl;
    ST7701S_WritePin_Fn write_sda;
    ST7701S_WritePin_Fn write_reset;
    ST7701S_DelayMs_Fn delay_ms;
    ST7701S_DelayUs_Fn delay_us;
    uint32_t serial_half_period_us;
} ST7701S_IO_t;

typedef struct
{
    ST7701S_IO_t io;
    uint8_t registered;
} ST7701S_Object_t;

typedef struct
{
    uint8_t command;
    const uint8_t *parameters;
    uint8_t parameter_count;
    uint16_t delay_after_ms;
} ST7701S_SequenceEntry_t;

ST7701S_Status_t ST7701S_RegisterIO(ST7701S_Object_t *object,
                                    const ST7701S_IO_t *io);
ST7701S_Status_t ST7701S_HardwareReset(ST7701S_Object_t *object);
ST7701S_Status_t ST7701S_WriteCommand(ST7701S_Object_t *object,
                                      uint8_t command,
                                      const uint8_t *parameters,
                                      size_t parameter_count);
ST7701S_Status_t ST7701S_RunSequence(ST7701S_Object_t *object,
                                     const ST7701S_SequenceEntry_t *sequence,
                                     size_t entry_count);
ST7701S_Status_t ST7701S_SleepIn(ST7701S_Object_t *object);
ST7701S_Status_t ST7701S_SleepOut(ST7701S_Object_t *object);
ST7701S_Status_t ST7701S_DisplayOff(ST7701S_Object_t *object);
ST7701S_Status_t ST7701S_DisplayOn(ST7701S_Object_t *object);

/** Apply the DWIN-provided register profile for the LI48480T028BA3098. */
ST7701S_Status_t ST7701S_InitDWIN_LI48480T028BA3098(
    ST7701S_Object_t *object);

#ifdef __cplusplus
}
#endif

#endif /* ST7701S_H */
