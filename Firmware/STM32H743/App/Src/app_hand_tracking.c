#include "app_hand_tracking.h"

#include <stddef.h>
#include "FreeRTOS.h"
#include "task.h"

#define APP_HAND_AXIS_LIMIT              10
#define APP_HAND_DEAD_ZONE               1
#define APP_HAND_MIN_DISTANCE_MM         10U
#define APP_HAND_ACTIVE_DISTANCE_MM      150U
#define APP_HAND_PRESENCE_DISTANCE_MM    500U
#define APP_HAND_TRACKING_MAX_DISTANCE_MM 1000U
#define APP_HAND_NO_TARGET_Z_MM          1001U
#define APP_HAND_SURFACE_WINDOW_MM       200U
#define APP_HAND_LOST_FRAME_HOLD_COUNT   3U

/* Change either value to 1 after the physical orientation test if an axis is
   mirrored by the sensor placement on the PCB. */
#define APP_HAND_FLIP_X                  0
#define APP_HAND_FLIP_Y                  1
#define APP_HAND_SWAP_XY                 0

static const int8_t app_hand_axis_coordinates[4] = {-10, -3, 3, 10};
static bool app_hand_filter_initialized;

/* Published only after a complete frame; the GUI never reads partially
 * updated diagnostic fields. Three periods of the current 5 Hz sensor. */
#define APP_HAND_POINTER_TIMEOUT_MS 600U
static struct
{
    TickType_t tick;
    int8_t x;
    int8_t y;
    bool pressed;
} app_hand_pointer;

static void APP_HandTracking_PublishPointer(void)
{
    taskENTER_CRITICAL();
    app_hand_pointer.x = app_hand_tracking.x;
    app_hand_pointer.y = app_hand_tracking.y;
    app_hand_pointer.pressed = app_hand_tracking.hand_active &&
                              (app_hand_tracking.lost_frame_count == 0U);
    app_hand_pointer.tick = xTaskGetTickCount();
    taskEXIT_CRITICAL();
}

bool APP_HandTracking_ReadPointer(int8_t *x, int8_t *y)
{
    bool pressed;
    if ((x == NULL) || (y == NULL))
    {
        return false;
    }

    taskENTER_CRITICAL();
    pressed = app_hand_pointer.pressed &&
        ((TickType_t)(xTaskGetTickCount() - app_hand_pointer.tick) <
         pdMS_TO_TICKS(APP_HAND_POINTER_TIMEOUT_MS));
    if (pressed)
    {
        *x = app_hand_pointer.x;
        *y = app_hand_pointer.y;
    }
    taskEXIT_CRITICAL();
    return pressed;
}

volatile APP_HandTracking_t app_hand_tracking = {
    .magic = 0x48414E44U, /* "HAND" */
    .status = APP_HAND_STATUS_NO_PERSON,
    .position_direction = APP_HAND_DIRECTION_CENTER,
    .movement_direction = APP_HAND_DIRECTION_CENTER,
    .z_mm = APP_HAND_NO_TARGET_Z_MM,
    .raw_z_mm = APP_HAND_NO_TARGET_Z_MM
};

volatile int32_t tof_x;
volatile int32_t tof_y;

static int8_t APP_HandTracking_ClampAxis(int32_t value)
{
    if (value > APP_HAND_AXIS_LIMIT)
    {
        return APP_HAND_AXIS_LIMIT;
    }
    if (value < -APP_HAND_AXIS_LIMIT)
    {
        return -APP_HAND_AXIS_LIMIT;
    }
    if ((value >= -APP_HAND_DEAD_ZONE) && (value <= APP_HAND_DEAD_ZONE))
    {
        return 0;
    }
    return (int8_t)value;
}

static bool APP_HandTracking_IsValidZone(const BSP_TOF_Data_t *data,
                                         uint32_t zone)
{
    const uint8_t status = data->target_status[zone];
    const int16_t distance = data->distance_mm[zone];

    return (data->targets_detected[zone] > 0U) &&
           ((status == 5U) || (status == 9U)) &&
           (distance > 0) &&
           (distance <= (int16_t)APP_HAND_TRACKING_MAX_DISTANCE_MM);
}

static APP_HandDirection_t APP_HandTracking_GetDirection(int8_t x, int8_t y)
{
    const bool horizontal = (x < -APP_HAND_DEAD_ZONE) ||
                            (x > APP_HAND_DEAD_ZONE);
    const bool vertical = (y < -APP_HAND_DEAD_ZONE) ||
                          (y > APP_HAND_DEAD_ZONE);

    if (!horizontal && !vertical)
    {
        return APP_HAND_DIRECTION_CENTER;
    }
    if (horizontal && vertical)
    {
        if (x > 0)
        {
            return (y > 0) ? APP_HAND_DIRECTION_UP_RIGHT
                           : APP_HAND_DIRECTION_DOWN_RIGHT;
        }
        return (y > 0) ? APP_HAND_DIRECTION_UP_LEFT
                       : APP_HAND_DIRECTION_DOWN_LEFT;
    }
    if (horizontal)
    {
        return (x > 0) ? APP_HAND_DIRECTION_RIGHT : APP_HAND_DIRECTION_LEFT;
    }
    return (y > 0) ? APP_HAND_DIRECTION_UP : APP_HAND_DIRECTION_DOWN;
}

static void APP_HandTracking_PublishNoTarget(void)
{
    if (app_hand_tracking.lost_frame_count < APP_HAND_LOST_FRAME_HOLD_COUNT)
    {
        ++app_hand_tracking.lost_frame_count;
        return;
    }

    app_hand_filter_initialized = false;
    app_hand_tracking.status = APP_HAND_STATUS_NO_PERSON;
    app_hand_tracking.person_present = false;
    app_hand_tracking.hand_active = false;
    app_hand_tracking.x = 0;
    app_hand_tracking.y = 0;
    app_hand_tracking.z_mm = APP_HAND_NO_TARGET_Z_MM;
    app_hand_tracking.raw_x = 0;
    app_hand_tracking.raw_y = 0;
    app_hand_tracking.raw_z_mm = APP_HAND_NO_TARGET_Z_MM;
    app_hand_tracking.delta_x = 0;
    app_hand_tracking.delta_y = 0;
    app_hand_tracking.delta_z_mm = 0;
    app_hand_tracking.menu_speed = 0U;
    app_hand_tracking.valid_zone_count = 0U;
    app_hand_tracking.confidence_percent = 0U;
    app_hand_tracking.position_direction = APP_HAND_DIRECTION_CENTER;
    app_hand_tracking.movement_direction = APP_HAND_DIRECTION_CENTER;
    tof_x = 0;
    tof_y = 0;
}

void APP_HandTracking_Reset(void)
{
    app_hand_filter_initialized = false;
    app_hand_tracking.update_count = 0U;
    app_hand_tracking.lost_frame_count = APP_HAND_LOST_FRAME_HOLD_COUNT;
    APP_HandTracking_PublishNoTarget();
    APP_HandTracking_PublishPointer();
}

void APP_HandTracking_Process(const BSP_TOF_Data_t *tof_data)
{
    uint16_t nearest_mm = APP_HAND_NO_TARGET_Z_MM;
    uint32_t weighted_x = 0U;
    uint32_t weighted_y = 0U;
    uint32_t weighted_z = 0U;
    uint32_t total_weight = 0U;
    uint8_t valid_zones = 0U;
    int8_t raw_x;
    int8_t raw_y;
    uint16_t raw_z;
    int8_t previous_x;
    int8_t previous_y;
    uint16_t previous_z;

    if (tof_data == NULL)
    {
        return;
    }

    for (uint32_t zone = 0U; zone < BSP_TOF_ZONE_COUNT; ++zone)
    {
        if (APP_HandTracking_IsValidZone(tof_data, zone) &&
            ((uint16_t)tof_data->distance_mm[zone] < nearest_mm))
        {
            nearest_mm = (uint16_t)tof_data->distance_mm[zone];
        }
    }

    if (nearest_mm > APP_HAND_TRACKING_MAX_DISTANCE_MM)
    {
        APP_HandTracking_PublishNoTarget();
        ++app_hand_tracking.update_count;
        APP_HandTracking_PublishPointer();
        return;
    }

    for (uint32_t zone = 0U; zone < BSP_TOF_ZONE_COUNT; ++zone)
    {
        const uint16_t distance = (uint16_t)tof_data->distance_mm[zone];
        const uint32_t row = zone / 4U;
        const uint32_t column = zone % 4U;
        uint32_t weight;
        int32_t zone_x;
        int32_t zone_y;

        if (!APP_HandTracking_IsValidZone(tof_data, zone) ||
            (distance > (nearest_mm + APP_HAND_SURFACE_WINDOW_MM)))
        {
            continue;
        }

        weight = (uint32_t)(nearest_mm + APP_HAND_SURFACE_WINDOW_MM - distance + 1U);
        zone_x = app_hand_axis_coordinates[column];
        zone_y = app_hand_axis_coordinates[3U - row];
#if APP_HAND_SWAP_XY
        {
            const int32_t temporary = zone_x;
            zone_x = zone_y;
            zone_y = temporary;
        }
#endif
#if APP_HAND_FLIP_X
        zone_x = -zone_x;
#endif
#if APP_HAND_FLIP_Y
        zone_y = -zone_y;
#endif
        weighted_x += (uint32_t)((zone_x + APP_HAND_AXIS_LIMIT) * (int32_t)weight);
        weighted_y += (uint32_t)((zone_y + APP_HAND_AXIS_LIMIT) * (int32_t)weight);
        weighted_z += (uint32_t)distance * weight;
        total_weight += weight;
        ++valid_zones;
    }

    if (total_weight == 0U)
    {
        APP_HandTracking_PublishNoTarget();
        ++app_hand_tracking.update_count;
        APP_HandTracking_PublishPointer();
        return;
    }

    raw_x = APP_HandTracking_ClampAxis(
        (int32_t)(weighted_x / total_weight) - APP_HAND_AXIS_LIMIT);
    raw_y = APP_HandTracking_ClampAxis(
        (int32_t)(weighted_y / total_weight) - APP_HAND_AXIS_LIMIT);
    raw_z = (uint16_t)(weighted_z / total_weight);
    if (raw_z <= APP_HAND_MIN_DISTANCE_MM)
    {
        raw_z = 0U;
    }

    previous_x = app_hand_tracking.x;
    previous_y = app_hand_tracking.y;
    previous_z = app_hand_tracking.z_mm;

    if (!app_hand_filter_initialized)
    {
        app_hand_tracking.x = raw_x;
        app_hand_tracking.y = raw_y;
        app_hand_tracking.z_mm = raw_z;
        app_hand_filter_initialized = true;
    }
    else
    {
        app_hand_tracking.x = APP_HandTracking_ClampAxis(
            ((int32_t)app_hand_tracking.x + raw_x) / 2);
        app_hand_tracking.y = APP_HandTracking_ClampAxis(
            ((int32_t)app_hand_tracking.y + raw_y) / 2);
        app_hand_tracking.z_mm =
            (uint16_t)(((uint32_t)app_hand_tracking.z_mm + raw_z) / 2U);
    }

    app_hand_tracking.raw_x = raw_x;
    app_hand_tracking.raw_y = raw_y;
    app_hand_tracking.raw_z_mm = raw_z;
    app_hand_tracking.delta_x = app_hand_tracking.x - previous_x;
    app_hand_tracking.delta_y = app_hand_tracking.y - previous_y;
    app_hand_tracking.delta_z_mm = (int16_t)((int32_t)app_hand_tracking.z_mm -
                                             (int32_t)previous_z);
    app_hand_tracking.valid_zone_count = valid_zones;
    app_hand_tracking.confidence_percent =
        (uint8_t)(((uint32_t)valid_zones * 100U) / BSP_TOF_ZONE_COUNT);
    app_hand_tracking.person_present =
        (app_hand_tracking.z_mm <= APP_HAND_PRESENCE_DISTANCE_MM);
    app_hand_tracking.hand_active =
        (raw_z < APP_HAND_ACTIVE_DISTANCE_MM);
    if (app_hand_tracking.hand_active)
    {
        app_hand_tracking.status = APP_HAND_STATUS_HAND_ACTIVE;
        tof_x = app_hand_tracking.x;
        tof_y = app_hand_tracking.y;
    }
    else if (app_hand_tracking.person_present)
    {
        app_hand_tracking.status = APP_HAND_STATUS_PERSON_NEAR;
        tof_x = 0;
        tof_y = 0;
    }
    else
    {
        app_hand_tracking.status = APP_HAND_STATUS_NO_PERSON;
        tof_x = 0;
        tof_y = 0;
    }
    app_hand_tracking.position_direction =
        APP_HandTracking_GetDirection(app_hand_tracking.x, app_hand_tracking.y);
    app_hand_tracking.movement_direction =
        APP_HandTracking_GetDirection(app_hand_tracking.delta_x,
                                      app_hand_tracking.delta_y);

    {
        const uint8_t abs_x = (uint8_t)((app_hand_tracking.x < 0)
                                            ? -app_hand_tracking.x
                                            : app_hand_tracking.x);
        app_hand_tracking.menu_speed = app_hand_tracking.hand_active
                                           ? abs_x
                                           : 0U;
    }

    app_hand_tracking.lost_frame_count = 0U;
    ++app_hand_tracking.update_count;
    APP_HandTracking_PublishPointer();
}
