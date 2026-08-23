#ifndef APP_HAND_TRACKING_H
#define APP_HAND_TRACKING_H

#include <stdbool.h>
#include <stdint.h>

#include "bsp_tof.h"

typedef enum
{
    APP_HAND_STATUS_NO_PERSON = 0,
    APP_HAND_STATUS_PERSON_NEAR,
    APP_HAND_STATUS_HAND_ACTIVE
} APP_HandStatus_t;

typedef enum
{
    APP_HAND_DIRECTION_CENTER = 0,
    APP_HAND_DIRECTION_LEFT,
    APP_HAND_DIRECTION_RIGHT,
    APP_HAND_DIRECTION_UP,
    APP_HAND_DIRECTION_DOWN,
    APP_HAND_DIRECTION_UP_LEFT,
    APP_HAND_DIRECTION_UP_RIGHT,
    APP_HAND_DIRECTION_DOWN_LEFT,
    APP_HAND_DIRECTION_DOWN_RIGHT
} APP_HandDirection_t;

typedef struct
{
    uint32_t magic;
    uint32_t update_count;
    APP_HandStatus_t status;
    APP_HandDirection_t position_direction;
    APP_HandDirection_t movement_direction;
    bool person_present;
    bool hand_active;
    int8_t x;
    int8_t y;
    uint16_t z_mm;
    int8_t raw_x;
    int8_t raw_y;
    uint16_t raw_z_mm;
    int8_t delta_x;
    int8_t delta_y;
    int16_t delta_z_mm;
    uint8_t menu_speed;
    uint8_t valid_zone_count;
    uint8_t confidence_percent;
    uint8_t lost_frame_count;
} APP_HandTracking_t;

extern volatile APP_HandTracking_t app_hand_tracking;

void APP_HandTracking_Reset(void);
void APP_HandTracking_Process(const BSP_TOF_Data_t *tof_data);

#endif
