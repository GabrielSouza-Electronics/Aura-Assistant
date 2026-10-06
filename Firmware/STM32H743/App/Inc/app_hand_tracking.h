#ifndef APP_HAND_TRACKING_H
#define APP_HAND_TRACKING_H

#include <stdbool.h>
#include <stdint.h>

#include "bsp_tof.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Tracker state, owned by SensorTask. The UI must use the snapshot API below;
 * the struct is exported only for host tests and debugger inspection. */
typedef struct
{
    int8_t x;
    int8_t y;                 /* Filtered -10..10; zero is neutral. */
    int8_t raw_x;
    int8_t raw_y;             /* Oriented nearest-depth centroid, unfiltered. */
    uint16_t raw_z_mm;        /* Nearest valid distance of the last frame. */
    uint8_t valid_zone_count; /* Zones tied at the nearest valid distance. */
    uint8_t lost_frame_count;
    bool hand_active;
} APP_HandTracking_t;

extern APP_HandTracking_t app_hand_tracking;

void APP_HandTracking_Reset(void);
void APP_HandTracking_Process(const BSP_TOF_Data_t *tof_data);

/* Task-context snapshot for the UI, positive right/up.
 * X is the filtered -10..10 coordinate: sign sets rotation direction and
 * magnitude sets speed. Y is filtered -10..10 with zero neutral.
 * Returns false for release, invalid targets or samples older than 600 ms.
 * Coordinates are only written while pressed. */
bool APP_HandTracking_ReadPointer(int8_t *x, int8_t *y);
/* Short approach <30 mm: emitted on release before 2 s. */
bool APP_HandTracking_TakeClick(void);
bool APP_HandTracking_ReadNear(void);
/* One event after a continuous <30 mm hold for 2 s; release rearms. */
bool APP_HandTracking_TakeBack(void);

#ifdef __cplusplus
}
#endif

#endif
