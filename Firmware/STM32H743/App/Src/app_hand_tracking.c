#include "app_hand_tracking.h"

#include <stddef.h>
#include "FreeRTOS.h"
#include "task.h"

#define APP_HAND_AXIS_LIMIT               10
#define APP_HAND_DEAD_ZONE                1
#define APP_HAND_MIN_DISTANCE_MM          10U
#define APP_HAND_ACTIVE_DISTANCE_MM       150U
#define APP_HAND_RELEASE_DISTANCE_MM      200U
#define APP_HAND_TRACKING_MAX_DISTANCE_MM 1000U
#define APP_HAND_NO_TARGET_Z_MM           1001U
#define APP_HAND_LOST_FRAME_HOLD_COUNT    3U
#define APP_HAND_CLICK_PRESS_MM           30U
#define APP_HAND_BACK_HOLD_MS             2000U
/* Three periods of the current 5 Hz sensor. */
#define APP_HAND_POINTER_TIMEOUT_MS       600U

/* Change either value to 1 after the physical orientation test if an axis is
   mirrored by the sensor placement on the PCB. */
#define APP_HAND_FLIP_X                   0
#define APP_HAND_FLIP_Y                   1
#define APP_HAND_SWAP_XY                  0

static const int8_t app_hand_axis_coordinates[4] = {-10, -3, 3, 10};
static bool app_hand_filter_initialized;

APP_HandTracking_t app_hand_tracking = {
    .raw_z_mm = APP_HAND_NO_TARGET_Z_MM
};

/* Published only after a complete frame; the GUI never sees a partially
 * updated tracker state. */
static struct
{
    TickType_t tick;
    int8_t x;
    int8_t y;
    bool pressed;
    bool click_pending;
    bool click_latched;
    bool near;
    bool back_pending;
    TickType_t near_tick;
} app_hand_pointer;

static void APP_HandTracking_PublishPointer(void)
{
    const bool fresh = (app_hand_tracking.lost_frame_count == 0U);

    taskENTER_CRITICAL();
    app_hand_pointer.x = app_hand_tracking.x;
    app_hand_pointer.y = app_hand_tracking.y;
    app_hand_pointer.pressed = app_hand_tracking.hand_active && fresh;
    if ((TickType_t)(xTaskGetTickCount() - app_hand_pointer.tick) >= pdMS_TO_TICKS(APP_HAND_POINTER_TIMEOUT_MS))
    {
        app_hand_pointer.near = false;
        app_hand_pointer.click_latched = false;
    }
    if (fresh)
    {
        const bool near = app_hand_pointer.pressed &&
                          app_hand_tracking.raw_z_mm < APP_HAND_CLICK_PRESS_MM;
        const TickType_t now = xTaskGetTickCount();
        if (near && !app_hand_pointer.near)
        {
            app_hand_pointer.near_tick = now;
            app_hand_pointer.click_latched = true;
        }
        if (near && app_hand_pointer.click_latched &&
            (TickType_t)(now - app_hand_pointer.near_tick) >= pdMS_TO_TICKS(APP_HAND_BACK_HOLD_MS))
        {
            app_hand_pointer.back_pending = true;
            app_hand_pointer.click_latched = false;
        }
        if (!near && app_hand_pointer.near && app_hand_pointer.click_latched)
            app_hand_pointer.click_pending =
                (TickType_t)(now - app_hand_pointer.near_tick) < pdMS_TO_TICKS(APP_HAND_BACK_HOLD_MS);
        if (!near) app_hand_pointer.click_latched = false;
        app_hand_pointer.near = near;
    }
    else
    {
        /* Invalid frames cancel a gesture; stale depth must never close a menu. */
        app_hand_pointer.near = false;
        app_hand_pointer.click_latched = false;
    }
    app_hand_pointer.tick = xTaskGetTickCount();
    taskEXIT_CRITICAL();
}

static bool APP_HandTracking_PointerFresh(void)
{
    return app_hand_pointer.pressed &&
           ((TickType_t)(xTaskGetTickCount() - app_hand_pointer.tick) <
            pdMS_TO_TICKS(APP_HAND_POINTER_TIMEOUT_MS));
}

bool APP_HandTracking_TakeClick(void)
{
    bool click;

    taskENTER_CRITICAL();
    click = app_hand_pointer.click_pending &&
            ((TickType_t)(xTaskGetTickCount() - app_hand_pointer.tick) < pdMS_TO_TICKS(APP_HAND_POINTER_TIMEOUT_MS));
    app_hand_pointer.click_pending = false;
    taskEXIT_CRITICAL();
    return click;
}

bool APP_HandTracking_ReadNear(void)
{
    taskENTER_CRITICAL();
    bool near = app_hand_pointer.near && APP_HandTracking_PointerFresh();
    taskEXIT_CRITICAL();
    return near;
}

bool APP_HandTracking_TakeBack(void)
{
    taskENTER_CRITICAL();
    bool back = app_hand_pointer.back_pending && APP_HandTracking_PointerFresh();
    app_hand_pointer.back_pending = false;
    taskEXIT_CRITICAL();
    return back;
}

bool APP_HandTracking_ReadPointer(int8_t *x, int8_t *y)
{
    bool pressed;

    if ((x == NULL) || (y == NULL))
    {
        return false;
    }

    taskENTER_CRITICAL();
    pressed = APP_HandTracking_PointerFresh();
    if (pressed)
    {
        *x = app_hand_pointer.x;
        *y = app_hand_pointer.y;
    }
    taskEXIT_CRITICAL();
    return pressed;
}

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

static void APP_HandTracking_ClearTarget(void)
{
    app_hand_filter_initialized = false;
    app_hand_tracking.hand_active = false;
    app_hand_tracking.x = 0;
    app_hand_tracking.y = 0;
    app_hand_tracking.raw_x = 0;
    app_hand_tracking.raw_y = 0;
    app_hand_tracking.raw_z_mm = APP_HAND_NO_TARGET_Z_MM;
    app_hand_tracking.valid_zone_count = 0U;
}

static void APP_HandTracking_ProcessNoTarget(bool withdrawn)
{
    /* Withdrawal may leave the ranging field entirely. Emit only a short
       release; invalid/stale data must never synthesize a long hold. */
    taskENTER_CRITICAL();
    if (withdrawn && app_hand_pointer.near && app_hand_pointer.click_latched &&
        (TickType_t)(xTaskGetTickCount() - app_hand_pointer.tick) < pdMS_TO_TICKS(APP_HAND_POINTER_TIMEOUT_MS) &&
        (TickType_t)(xTaskGetTickCount() - app_hand_pointer.near_tick) < pdMS_TO_TICKS(APP_HAND_BACK_HOLD_MS))
        app_hand_pointer.click_pending = true;
    taskEXIT_CRITICAL();
    /* Hold the last target for a few frames to ride over single dropouts. */
    if (app_hand_tracking.lost_frame_count < APP_HAND_LOST_FRAME_HOLD_COUNT)
    {
        ++app_hand_tracking.lost_frame_count;
    }
    else
    {
        APP_HandTracking_ClearTarget();
    }
    APP_HandTracking_PublishPointer();
}

void APP_HandTracking_Reset(void)
{
    taskENTER_CRITICAL();
    app_hand_pointer.click_latched = false;
    app_hand_pointer.click_pending = false;
    app_hand_pointer.near = false;
    app_hand_pointer.back_pending = false;
    taskEXIT_CRITICAL();
    app_hand_tracking.lost_frame_count = APP_HAND_LOST_FRAME_HOLD_COUNT;
    APP_HandTracking_ClearTarget();
    APP_HandTracking_PublishPointer();
}

void APP_HandTracking_Process(const BSP_TOF_Data_t *tof_data)
{
    uint16_t nearest_mm = APP_HAND_NO_TARGET_Z_MM;
    int32_t sum_x = 0;
    int32_t sum_y = 0;
    uint8_t valid_zones = 0U;
    uint16_t raw_z;

    if (tof_data == NULL)
    {
        return;
    }

    /* Only the nearest depth controls the pointer: farther palm zones must
       not pull it away from the fingertip. Equal minima share their centroid
       because one frame cannot distinguish them. */
    for (uint32_t zone = 0U; zone < BSP_TOF_ZONE_COUNT; ++zone)
    {
        uint16_t distance;
        int32_t zone_x;
        int32_t zone_y;

        if (!APP_HandTracking_IsValidZone(tof_data, zone))
        {
            continue;
        }
        distance = (uint16_t)tof_data->distance_mm[zone];
        if (distance > nearest_mm)
        {
            continue;
        }
        if (distance < nearest_mm)
        {
            nearest_mm = distance;
            sum_x = 0;
            sum_y = 0;
            valid_zones = 0U;
        }

        zone_x = app_hand_axis_coordinates[zone % 4U];
        zone_y = app_hand_axis_coordinates[3U - (zone / 4U)];
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
        sum_x += zone_x;
        sum_y += zone_y;
        ++valid_zones;
    }

    if (valid_zones == 0U)
    {
        bool withdrawn = true;
        for (uint32_t zone = 0; zone < BSP_TOF_ZONE_COUNT; ++zone)
            if (tof_data->targets_detected[zone]) withdrawn = false;
        APP_HandTracking_ProcessNoTarget(withdrawn);
        return;
    }

    app_hand_tracking.raw_x = APP_HandTracking_ClampAxis(sum_x / (int32_t)valid_zones);
    app_hand_tracking.raw_y = APP_HandTracking_ClampAxis(sum_y / (int32_t)valid_zones);
    raw_z = (nearest_mm <= APP_HAND_MIN_DISTANCE_MM) ? 0U : nearest_mm;
    app_hand_tracking.raw_z_mm = raw_z;
    app_hand_tracking.valid_zone_count = valid_zones;

    if (!app_hand_filter_initialized)
    {
        app_hand_tracking.x = app_hand_tracking.raw_x;
        app_hand_tracking.y = app_hand_tracking.raw_y;
        app_hand_filter_initialized = true;
    }
    else
    {
        app_hand_tracking.x = APP_HandTracking_ClampAxis(
            ((int32_t)app_hand_tracking.x + app_hand_tracking.raw_x) / 2);
        app_hand_tracking.y = APP_HandTracking_ClampAxis(
            ((int32_t)app_hand_tracking.y + app_hand_tracking.raw_y) / 2);
    }

    /* Enter below 150 mm; once active, release only above 200 mm. Uses the
       unfiltered depth so selection reacts without filter delay. */
    app_hand_tracking.hand_active = app_hand_tracking.hand_active
        ? (raw_z <= APP_HAND_RELEASE_DISTANCE_MM)
        : (raw_z < APP_HAND_ACTIVE_DISTANCE_MM);
    app_hand_tracking.lost_frame_count = 0U;
    APP_HandTracking_PublishPointer();
}
