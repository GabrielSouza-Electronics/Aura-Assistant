#ifndef WS2812C_H
#define WS2812C_H

#include <stddef.h>
#include <stdint.h>

typedef enum
{
    WS2812C_OK = 0,
    WS2812C_INVALID_ARGUMENT,
    WS2812C_INDEX_OUT_OF_RANGE,
    WS2812C_BUFFER_TOO_SMALL
} WS2812C_Status_t;

typedef struct
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} WS2812C_Color_t;

typedef struct
{
    WS2812C_Color_t *pixels;
    size_t pixel_count;
} WS2812C_Handle_t;

#define WS2812C_BITS_PER_PIXEL 24U

WS2812C_Status_t WS2812C_Init(WS2812C_Handle_t *handle,
                              WS2812C_Color_t *pixel_storage,
                              size_t pixel_count);
WS2812C_Status_t WS2812C_SetPixel(WS2812C_Handle_t *handle,
                                  size_t index,
                                  WS2812C_Color_t color);
WS2812C_Status_t WS2812C_Fill(WS2812C_Handle_t *handle,
                              WS2812C_Color_t color);
WS2812C_Status_t WS2812C_EncodePwm(const WS2812C_Handle_t *handle,
                                   uint16_t zero_high_ticks,
                                   uint16_t one_high_ticks,
                                   size_t reset_slots,
                                   uint16_t *output,
                                   size_t output_capacity,
                                   size_t *output_length);

#endif
