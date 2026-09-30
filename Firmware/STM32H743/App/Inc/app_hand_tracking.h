#ifndef APP_HAND_TRACKING_H
#define APP_HAND_TRACKING_H

#include <stdbool.h>
#include <stdint.h>

#include "bsp_tof.h"

#ifdef __cplusplus
extern "C" {
#endif

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
    int8_t y; /* Filtered -10..10; zero is neutral. */
    uint16_t z_mm;
    int8_t raw_x;
    int8_t raw_y; /* Oriented nearest-depth centroid, before temporal filtering. */
    uint16_t raw_z_mm;
    int8_t delta_x;
    int8_t delta_y;
    int16_t delta_z_mm;
    uint8_t menu_speed; /* abs(filtered X), 0..10; zero when hand is inactive. */
    uint8_t valid_zone_count; /* Zones tied at the nearest valid distance. */
    uint8_t confidence_percent; /* Selected-zone coverage, not target certainty. */
    uint8_t lost_frame_count;
} APP_HandTracking_t;

extern volatile APP_HandTracking_t app_hand_tracking;

/* Filtered debugger coordinates (-10..10); zero when no hand is active. */
extern volatile int32_t tof_x;
extern volatile int32_t tof_y;

void APP_HandTracking_Reset(void);
void APP_HandTracking_Process(const BSP_TOF_Data_t *tof_data);

/* Task-context snapshot for the UI, positive right/up.
 * X is the filtered -10..10 coordinate: sign sets rotation direction and
 * magnitude sets speed. Y is filtered -10..10 with zero neutral.
 * Returns false for release, invalid targets or samples older than 600 ms.
 * Coordinates are only written while pressed. */
bool APP_HandTracking_ReadPointer(int8_t *x, int8_t *y);
/* Consume one fresh approach below 40 mm; rearmed by a valid >=50 mm sample. */
bool APP_HandTracking_TakeClick(void);

#ifdef __cplusplus
}
#endif

#endif
