#include "st7701s.h"

#include <limits.h>
#include <string.h>

#define ST7701S_PIN_LOW                 0U
#define ST7701S_PIN_HIGH                1U
#define ST7701S_COMMAND_FRAME_PREFIX    0U
#define ST7701S_DATA_FRAME_PREFIX       1U
#define ST7701S_DEFAULT_HALF_PERIOD_US  1U
#define ST7701S_RESET_ASSERT_MS         10U
#define ST7701S_RESET_RECOVERY_MS       120U

static ST7701S_Status_t ST7701S_WritePin(ST7701S_WritePin_Fn function,
                                         void *context,
                                         uint8_t level)
{
    return (function(context, level) == 0) ? ST7701S_OK : ST7701S_ERROR_IO;
}

static void ST7701S_DelayHalfPeriod(const ST7701S_Object_t *object)
{
    uint32_t delay_us = object->io.serial_half_period_us;

    if (delay_us == 0U)
    {
        delay_us = ST7701S_DEFAULT_HALF_PERIOD_US;
    }
    object->io.delay_us(object->io.context, delay_us);
}

/*
 * ST7701S 3-line serial write frame: D/CX followed by D7..D0, MSB first.
 * SCL idles low and the controller samples SDA on the rising clock edge.
 */
static ST7701S_Status_t ST7701S_Write9BitFrame(ST7701S_Object_t *object,
                                               uint8_t data_prefix,
                                               uint8_t value)
{
    uint16_t frame = (uint16_t)(((uint16_t)data_prefix << 8U) | value);
    int32_t bit;

    for (bit = 8; bit >= 0; --bit)
    {
        ST7701S_Status_t status;
        uint8_t level = (uint8_t)((frame >> (uint32_t)bit) & 0x01U);

        status = ST7701S_WritePin(object->io.write_sda, object->io.context, level);
        if (status != ST7701S_OK)
        {
            return status;
        }

        ST7701S_DelayHalfPeriod(object);
        status = ST7701S_WritePin(object->io.write_scl,
                                  object->io.context,
                                  ST7701S_PIN_HIGH);
        if (status != ST7701S_OK)
        {
            return status;
        }

        ST7701S_DelayHalfPeriod(object);
        status = ST7701S_WritePin(object->io.write_scl,
                                  object->io.context,
                                  ST7701S_PIN_LOW);
        if (status != ST7701S_OK)
        {
            return status;
        }
    }

    return ST7701S_OK;
}

ST7701S_Status_t ST7701S_RegisterIO(ST7701S_Object_t *object,
                                    const ST7701S_IO_t *io)
{
    if ((object == NULL) || (io == NULL) ||
        (io->write_cs == NULL) || (io->write_scl == NULL) ||
        (io->write_sda == NULL) || (io->write_reset == NULL) ||
        (io->delay_ms == NULL) || (io->delay_us == NULL))
    {
        return ST7701S_ERROR_INVALID_ARGUMENT;
    }

    (void)memset(object, 0, sizeof(*object));
    object->io = *io;
    object->registered = 1U;

    if ((ST7701S_WritePin(object->io.write_cs, object->io.context,
                          ST7701S_PIN_HIGH) != ST7701S_OK) ||
        (ST7701S_WritePin(object->io.write_scl, object->io.context,
                          ST7701S_PIN_LOW) != ST7701S_OK) ||
        (ST7701S_WritePin(object->io.write_sda, object->io.context,
                          ST7701S_PIN_LOW) != ST7701S_OK))
    {
        object->registered = 0U;
        return ST7701S_ERROR_IO;
    }

    return ST7701S_OK;
}

ST7701S_Status_t ST7701S_HardwareReset(ST7701S_Object_t *object)
{
    if ((object == NULL) || (object->registered == 0U))
    {
        return ST7701S_ERROR_NOT_INITIALIZED;
    }

    if (ST7701S_WritePin(object->io.write_reset, object->io.context,
                         ST7701S_PIN_HIGH) != ST7701S_OK)
    {
        return ST7701S_ERROR_IO;
    }
    object->io.delay_ms(object->io.context, ST7701S_RESET_ASSERT_MS);

    if (ST7701S_WritePin(object->io.write_reset, object->io.context,
                         ST7701S_PIN_LOW) != ST7701S_OK)
    {
        return ST7701S_ERROR_IO;
    }
    object->io.delay_ms(object->io.context, ST7701S_RESET_ASSERT_MS);

    if (ST7701S_WritePin(object->io.write_reset, object->io.context,
                         ST7701S_PIN_HIGH) != ST7701S_OK)
    {
        return ST7701S_ERROR_IO;
    }
    object->io.delay_ms(object->io.context, ST7701S_RESET_RECOVERY_MS);

    return ST7701S_OK;
}

ST7701S_Status_t ST7701S_WriteCommand(ST7701S_Object_t *object,
                                      uint8_t command,
                                      const uint8_t *parameters,
                                      size_t parameter_count)
{
    ST7701S_Status_t status;
    size_t index;

    if ((object == NULL) || (object->registered == 0U))
    {
        return ST7701S_ERROR_NOT_INITIALIZED;
    }
    if (((parameters == NULL) && (parameter_count != 0U)) ||
        (parameter_count > UINT8_MAX))
    {
        return ST7701S_ERROR_INVALID_ARGUMENT;
    }

    status = ST7701S_WritePin(object->io.write_cs, object->io.context,
                              ST7701S_PIN_LOW);
    if (status == ST7701S_OK)
    {
        status = ST7701S_Write9BitFrame(object, ST7701S_COMMAND_FRAME_PREFIX,
                                        command);
    }

    for (index = 0U; (index < parameter_count) && (status == ST7701S_OK); ++index)
    {
        status = ST7701S_Write9BitFrame(object, ST7701S_DATA_FRAME_PREFIX,
                                        parameters[index]);
    }

    if (ST7701S_WritePin(object->io.write_cs, object->io.context,
                         ST7701S_PIN_HIGH) != ST7701S_OK)
    {
        status = ST7701S_ERROR_IO;
    }

    return status;
}

ST7701S_Status_t ST7701S_RunSequence(ST7701S_Object_t *object,
                                     const ST7701S_SequenceEntry_t *sequence,
                                     size_t entry_count)
{
    size_t index;

    if ((object == NULL) || (object->registered == 0U))
    {
        return ST7701S_ERROR_NOT_INITIALIZED;
    }
    if ((sequence == NULL) && (entry_count != 0U))
    {
        return ST7701S_ERROR_INVALID_ARGUMENT;
    }

    for (index = 0U; index < entry_count; ++index)
    {
        ST7701S_Status_t status = ST7701S_WriteCommand(
            object,
            sequence[index].command,
            sequence[index].parameters,
            sequence[index].parameter_count);

        if (status != ST7701S_OK)
        {
            return status;
        }
        if (sequence[index].delay_after_ms != 0U)
        {
            object->io.delay_ms(object->io.context,
                                sequence[index].delay_after_ms);
        }
    }

    return ST7701S_OK;
}

ST7701S_Status_t ST7701S_SleepIn(ST7701S_Object_t *object)
{
    ST7701S_Status_t status = ST7701S_WriteCommand(
        object, ST7701S_CMD_SLEEP_IN, NULL, 0U);

    if (status == ST7701S_OK)
    {
        object->io.delay_ms(object->io.context, 120U);
    }
    return status;
}

ST7701S_Status_t ST7701S_SleepOut(ST7701S_Object_t *object)
{
    ST7701S_Status_t status = ST7701S_WriteCommand(
        object, ST7701S_CMD_SLEEP_OUT, NULL, 0U);

    if (status == ST7701S_OK)
    {
        object->io.delay_ms(object->io.context, 120U);
    }
    return status;
}

ST7701S_Status_t ST7701S_DisplayOff(ST7701S_Object_t *object)
{
    return ST7701S_WriteCommand(object, ST7701S_CMD_DISPLAY_OFF, NULL, 0U);
}

ST7701S_Status_t ST7701S_DisplayOn(ST7701S_Object_t *object)
{
    return ST7701S_WriteCommand(object, ST7701S_CMD_DISPLAY_ON, NULL, 0U);
}

/*
 * Panel-specific register profile supplied by DWIN for LI48480T028BA3098.
 * Source: DWIN initialization sequence received by Gabriel Souza, 2026-08-11.
 * Values and command order below intentionally match the supplied sequence.
 */
static const uint8_t page_13[] = {0x77U, 0x01U, 0x00U, 0x00U, 0x13U};
static const uint8_t page_13_command[] = {0x08U};
static const uint8_t page_10[] = {0x77U, 0x01U, 0x00U, 0x00U, 0x10U};
static const uint8_t line_setting[] = {0x3BU, 0x00U};
static const uint8_t porch_setting[] = {0x10U, 0x0CU};
static const uint8_t inversion_setting[] = {0x07U, 0x0AU};
/* Physical RGB panel scan orientation requested for the assembled display.
   TouchGFX coordinates remain unchanged. */
static const uint8_t x_direction[] = {0x04U};
static const uint8_t panel_control[] = {0x10U};
static const uint8_t positive_gamma[] = {
    0x05U, 0x12U, 0x98U, 0x0EU, 0x0FU, 0x07U, 0x07U, 0x09U,
    0x09U, 0x23U, 0x05U, 0x52U, 0x0FU, 0x67U, 0x2CU, 0x11U
};
static const uint8_t negative_gamma[] = {
    0x0BU, 0x11U, 0x97U, 0x0CU, 0x12U, 0x06U, 0x06U, 0x08U,
    0x08U, 0x22U, 0x03U, 0x51U, 0x11U, 0x66U, 0x2BU, 0x0FU
};
static const uint8_t page_11[] = {0x77U, 0x01U, 0x00U, 0x00U, 0x11U};
static const uint8_t vop_setting[] = {0x5DU};
static const uint8_t vcom_setting[] = {0x35U};
static const uint8_t vgh_setting[] = {0x81U};
static const uint8_t test_command[] = {0x80U};
static const uint8_t vgl_setting[] = {0x4EU};
static const uint8_t power_control_1[] = {0x85U};
static const uint8_t power_control_2[] = {0x20U};
static const uint8_t avdd_setting[] = {0x78U};
static const uint8_t avcl_setting[] = {0x78U};
static const uint8_t power_control_3[] = {0x88U};
static const uint8_t source_control[] = {0x00U, 0x00U, 0x02U};
static const uint8_t source_equalize[] = {
    0x06U, 0x30U, 0x08U, 0x30U, 0x05U, 0x30U,
    0x07U, 0x30U, 0x00U, 0x33U, 0x33U
};
static const uint8_t gate_control_1[] = {
    0x11U, 0x11U, 0x33U, 0x33U, 0xF4U, 0x00U,
    0x00U, 0x00U, 0xF4U, 0x00U, 0x00U, 0x00U
};
static const uint8_t gate_control_2[] = {0x00U, 0x00U, 0x11U, 0x11U};
static const uint8_t gate_control_3[] = {0x44U, 0x44U};
static const uint8_t gate_control_4[] = {
    0x0DU, 0xF5U, 0x30U, 0xF0U, 0x0FU, 0xF7U, 0x30U, 0xF0U,
    0x09U, 0xF1U, 0x30U, 0xF0U, 0x0BU, 0xF3U, 0x30U, 0xF0U
};
static const uint8_t gate_control_5[] = {0x00U, 0x00U, 0x11U, 0x11U};
static const uint8_t gate_control_6[] = {0x44U, 0x44U};
static const uint8_t gate_control_7[] = {
    0x0CU, 0xF4U, 0x30U, 0xF0U, 0x0EU, 0xF6U, 0x30U, 0xF0U,
    0x08U, 0xF0U, 0x30U, 0xF0U, 0x0AU, 0xF2U, 0x30U, 0xF0U
};
static const uint8_t gate_control_e9[] = {0x36U, 0x01U};
static const uint8_t gate_control_eb[] = {
    0x00U, 0x01U, 0xE4U, 0xE4U, 0x44U, 0x88U, 0x40U
};
static const uint8_t gate_control_ed[] = {
    0xFFU, 0x10U, 0xBFU, 0x76U, 0x54U, 0x2AU, 0xFCU, 0xFFU,
    0xFFU, 0xCFU, 0xA2U, 0x45U, 0x67U, 0xFBU, 0x01U, 0xFFU
};
static const uint8_t gate_control_ef[] = {
    0x08U, 0x08U, 0x08U, 0x45U, 0x3FU, 0x54U
};
static const uint8_t page_00[] = {0x77U, 0x01U, 0x00U, 0x00U, 0x00U};
/* Validated stable RGB-mode setting from the DWIN panel profile. */
static const uint8_t address_mode[] = {0x18U};
static const uint8_t pixel_format[] = {ST7701S_PIXEL_FORMAT_RGB888};
static const uint8_t tearing_effect[] = {0x00U};

#define ST7701S_SEQUENCE_ENTRY(command_, data_, delay_) \
    { (command_), (data_), (uint8_t)sizeof(data_), (delay_) }
#define ST7701S_SEQUENCE_COMMAND(command_, delay_) \
    { (command_), NULL, 0U, (delay_) }

static const ST7701S_SequenceEntry_t dwin_li48480t028ba3098_sequence[] = {
    ST7701S_SEQUENCE_ENTRY(0xFFU, page_13, 0U),
    ST7701S_SEQUENCE_ENTRY(0xEFU, page_13_command, 0U),
    ST7701S_SEQUENCE_ENTRY(0xFFU, page_10, 0U),
    ST7701S_SEQUENCE_ENTRY(0xC0U, line_setting, 0U),
    ST7701S_SEQUENCE_ENTRY(0xC1U, porch_setting, 0U),
    ST7701S_SEQUENCE_ENTRY(0xC2U, inversion_setting, 0U),
    ST7701S_SEQUENCE_ENTRY(0xC7U, x_direction, 0U),
    ST7701S_SEQUENCE_ENTRY(0xCCU, panel_control, 0U),
    ST7701S_SEQUENCE_ENTRY(0xB0U, positive_gamma, 0U),
    ST7701S_SEQUENCE_ENTRY(0xB1U, negative_gamma, 0U),
    ST7701S_SEQUENCE_ENTRY(0xFFU, page_11, 0U),
    ST7701S_SEQUENCE_ENTRY(0xB0U, vop_setting, 0U),
    ST7701S_SEQUENCE_ENTRY(0xB1U, vcom_setting, 0U),
    ST7701S_SEQUENCE_ENTRY(0xB2U, vgh_setting, 0U),
    ST7701S_SEQUENCE_ENTRY(0xB3U, test_command, 0U),
    ST7701S_SEQUENCE_ENTRY(0xB5U, vgl_setting, 0U),
    ST7701S_SEQUENCE_ENTRY(0xB7U, power_control_1, 0U),
    ST7701S_SEQUENCE_ENTRY(0xB8U, power_control_2, 0U),
    ST7701S_SEQUENCE_ENTRY(0xC1U, avdd_setting, 0U),
    ST7701S_SEQUENCE_ENTRY(0xC2U, avcl_setting, 0U),
    ST7701S_SEQUENCE_ENTRY(0xD0U, power_control_3, 0U),
    ST7701S_SEQUENCE_ENTRY(0xE0U, source_control, 0U),
    ST7701S_SEQUENCE_ENTRY(0xE1U, source_equalize, 0U),
    ST7701S_SEQUENCE_ENTRY(0xE2U, gate_control_1, 0U),
    ST7701S_SEQUENCE_ENTRY(0xE3U, gate_control_2, 0U),
    ST7701S_SEQUENCE_ENTRY(0xE4U, gate_control_3, 0U),
    ST7701S_SEQUENCE_ENTRY(0xE5U, gate_control_4, 0U),
    ST7701S_SEQUENCE_ENTRY(0xE6U, gate_control_5, 0U),
    ST7701S_SEQUENCE_ENTRY(0xE7U, gate_control_6, 0U),
    ST7701S_SEQUENCE_ENTRY(0xE8U, gate_control_7, 0U),
    ST7701S_SEQUENCE_ENTRY(0xE9U, gate_control_e9, 0U),
    ST7701S_SEQUENCE_ENTRY(0xEBU, gate_control_eb, 0U),
    ST7701S_SEQUENCE_ENTRY(0xEDU, gate_control_ed, 0U),
    ST7701S_SEQUENCE_ENTRY(0xEFU, gate_control_ef, 0U),
    ST7701S_SEQUENCE_ENTRY(0xFFU, page_00, 0U),
    ST7701S_SEQUENCE_COMMAND(ST7701S_CMD_SLEEP_OUT, 120U),
    ST7701S_SEQUENCE_ENTRY(ST7701S_CMD_PIXEL_FORMAT, pixel_format, 0U),
    ST7701S_SEQUENCE_ENTRY(ST7701S_CMD_ADDRESS_MODE, address_mode, 0U),
    ST7701S_SEQUENCE_ENTRY(0x35U, tearing_effect, 0U),
    ST7701S_SEQUENCE_COMMAND(ST7701S_CMD_DISPLAY_ON, 0U)
};

ST7701S_Status_t ST7701S_InitDWIN_LI48480T028BA3098(
    ST7701S_Object_t *object)
{
    ST7701S_Status_t status = ST7701S_HardwareReset(object);

    if (status != ST7701S_OK)
    {
        return status;
    }

    return ST7701S_RunSequence(
        object,
        dwin_li48480t028ba3098_sequence,
        sizeof(dwin_li48480t028ba3098_sequence) /
            sizeof(dwin_li48480t028ba3098_sequence[0]));
}
