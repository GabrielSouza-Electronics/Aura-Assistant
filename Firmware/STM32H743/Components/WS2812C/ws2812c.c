#include "ws2812c.h"

WS2812C_Status_t WS2812C_Init(WS2812C_Handle_t *handle,
                              WS2812C_Color_t *pixel_storage,
                              size_t pixel_count)
{
    if ((handle == NULL) || (pixel_storage == NULL) || (pixel_count == 0U))
    {
        return WS2812C_INVALID_ARGUMENT;
    }

    handle->pixels = pixel_storage;
    handle->pixel_count = pixel_count;

    return WS2812C_Fill(handle, (WS2812C_Color_t){0U, 0U, 0U});
}

WS2812C_Status_t WS2812C_SetPixel(WS2812C_Handle_t *handle,
                                  size_t index,
                                  WS2812C_Color_t color)
{
    if ((handle == NULL) || (handle->pixels == NULL))
    {
        return WS2812C_INVALID_ARGUMENT;
    }
    if (index >= handle->pixel_count)
    {
        return WS2812C_INDEX_OUT_OF_RANGE;
    }

    handle->pixels[index] = color;
    return WS2812C_OK;
}

WS2812C_Status_t WS2812C_Fill(WS2812C_Handle_t *handle,
                              WS2812C_Color_t color)
{
    size_t index;

    if ((handle == NULL) || (handle->pixels == NULL) ||
        (handle->pixel_count == 0U))
    {
        return WS2812C_INVALID_ARGUMENT;
    }

    for (index = 0U; index < handle->pixel_count; ++index)
    {
        handle->pixels[index] = color;
    }

    return WS2812C_OK;
}

static size_t WS2812C_EncodeByte(uint8_t value,
                                 uint16_t zero_high_ticks,
                                 uint16_t one_high_ticks,
                                 uint16_t *output)
{
    uint8_t mask;
    size_t output_index = 0U;

    for (mask = 0x80U; mask != 0U; mask >>= 1U)
    {
        output[output_index++] = ((value & mask) != 0U) ?
                                 one_high_ticks : zero_high_ticks;
    }

    return output_index;
}

WS2812C_Status_t WS2812C_EncodePwm(const WS2812C_Handle_t *handle,
                                   uint16_t zero_high_ticks,
                                   uint16_t one_high_ticks,
                                   size_t reset_slots,
                                   uint16_t *output,
                                   size_t output_capacity,
                                   size_t *output_length)
{
    const size_t required_length =
        ((handle != NULL) ? handle->pixel_count : 0U) *
        WS2812C_BITS_PER_PIXEL + reset_slots;
    size_t output_index = 0U;
    size_t pixel_index;

    if ((handle == NULL) || (handle->pixels == NULL) ||
        (handle->pixel_count == 0U) || (output == NULL) ||
        (output_length == NULL) || (zero_high_ticks == 0U) ||
        (one_high_ticks <= zero_high_ticks))
    {
        return WS2812C_INVALID_ARGUMENT;
    }
    if (output_capacity < required_length)
    {
        return WS2812C_BUFFER_TOO_SMALL;
    }

    for (pixel_index = 0U; pixel_index < handle->pixel_count; ++pixel_index)
    {
        const WS2812C_Color_t color = handle->pixels[pixel_index];

        /* WS2812C wire format is GRB, most-significant bit first. */
        output_index += WS2812C_EncodeByte(color.green, zero_high_ticks,
                                           one_high_ticks, &output[output_index]);
        output_index += WS2812C_EncodeByte(color.red, zero_high_ticks,
                                           one_high_ticks, &output[output_index]);
        output_index += WS2812C_EncodeByte(color.blue, zero_high_ticks,
                                           one_high_ticks, &output[output_index]);
    }

    while (output_index < required_length)
    {
        output[output_index++] = 0U;
    }

    *output_length = output_index;
    return WS2812C_OK;
}
