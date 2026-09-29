#include "app.h"
#include "app_provisioning.h"

#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"
#include "w6x_api.h"

#include <limits.h>
#include <string.h>

#define APP_WIFI_BLE_RX_BUFFER_SIZE 256U

/* Display signal bars. UI policy, not radio limits: common RSSI buckets. */
#define APP_WIFI_SIGNAL_POLL_MS 3000U
#define APP_WIFI_RSSI_3_BARS (-60)
#define APP_WIFI_RSSI_2_BARS (-70)

volatile APP_WiFiDiagnostics_t app_wifi_diagnostics = {
    .magic = 0x57494649U, /* "WIFI" */
    .state = APP_WIFI_STATE_OFF,
    .wifi_best_rssi = INT32_MIN,
    .wifi_rssi = INT32_MIN,
    /* UINT32_MAX = not executed, distinct from successful status zero. */
    .core_status = UINT32_MAX,
    .callback_status = UINT32_MAX,
    .wifi_init_status = UINT32_MAX,
    .net_init_status = UINT32_MAX,
    .wifi_scan_status = UINT32_MAX,
    .wifi_scan_callback_status = UINT32_MAX,
    .ble_init_status = UINT32_MAX,
    .provision_init_status = UINT32_MAX,
    .ble_address_status = UINT32_MAX,
    .ble_adv_status = UINT32_MAX,
    .last_driver_error = UINT32_MAX,
    .first_driver_error = UINT32_MAX
};

static uint8_t app_wifi_ble_rx_buffer[APP_WIFI_BLE_RX_BUFFER_SIZE];
static volatile uint8_t app_wifi_signal_level;

uint8_t APP_WiFi_GetSignalLevel(void)
{
    return app_wifi_signal_level;
}

static void APP_WiFiSetSignalLevel(uint8_t level)
{
    app_wifi_signal_level = level;
    app_wifi_diagnostics.wifi_signal_level = level;
}

/* Runs only in APP_WiFiTask after W6X Wi-Fi/Net init succeeded: the W6X
 * asserts are compiled out, so an uninitialized module would fault. */
static void APP_WiFiUpdateSignal(void)
{
    W6X_WiFi_StaStateType_e state = W6X_WIFI_STATE_STA_OFF;
    W6X_WiFi_Connect_t info = {0};

    if (W6X_WiFi_Station_GetState(&state, &info) != W6X_STATUS_OK)
    {
        return; /* keep the last level: a transient AT error must not flicker */
    }
    if (state != W6X_WIFI_STATE_STA_GOT_IP && state != W6X_WIFI_STATE_STA_CONNECTED)
    {
        app_wifi_diagnostics.wifi_rssi = INT32_MIN;
        APP_WiFiSetSignalLevel(0U);
        return;
    }
    app_wifi_diagnostics.wifi_rssi = info.Rssi;
    APP_WiFiSetSignalLevel((info.Rssi >= APP_WIFI_RSSI_3_BARS) ? 3U :
                           (info.Rssi >= APP_WIFI_RSSI_2_BARS) ? 2U : 1U);
}

static void APP_WiFiEventCallback(W6X_event_id_t event_id, void *event_args)
{
    (void)event_args;
    app_wifi_diagnostics.last_wifi_event = event_id;
    APP_ProvisionWiFiEvent();

    switch (event_id)
    {
        case W6X_WIFI_EVT_CONNECTED_ID:
            app_wifi_diagnostics.wifi_connected = true;
            break;
        case W6X_WIFI_EVT_GOT_IP_ID:
            app_wifi_diagnostics.wifi_has_ip = true;
            break;
        case W6X_WIFI_EVT_DISCONNECTED_ID:
            app_wifi_diagnostics.wifi_connected = false;
            app_wifi_diagnostics.wifi_has_ip = false;
            break;
        default:
            break;
    }
}

/* W6X_Net_Init refuses to start without a registered Net callback. Sockets are
 * not used yet; only the last event is kept for diagnostics. */
static void APP_NetEventCallback(W6X_event_id_t event_id, void *event_args)
{
    (void)event_args;
    app_wifi_diagnostics.last_net_event = event_id;
}

static void APP_BleEventCallback(W6X_event_id_t event_id, void *event_args)
{
    APP_ProvisionBleEvent(event_id, event_args, app_wifi_ble_rx_buffer,
                          sizeof(app_wifi_ble_rx_buffer));
    app_wifi_diagnostics.last_ble_event = event_id;

    if (event_id == W6X_BLE_EVT_CONNECTED_ID)
    {
        app_wifi_diagnostics.ble_connected = true;
        ++app_wifi_diagnostics.ble_connection_count;
    }
    else if (event_id == W6X_BLE_EVT_DISCONNECTED_ID)
    {
        app_wifi_diagnostics.ble_connected = false;
    }
}

static void APP_WiFiDriverErrorCallback(W6X_Status_t status,
                                        char const *function_name)
{
    /* TranslateErrorStatus supplies __func__ strings with static lifetime. */
    if (app_wifi_diagnostics.first_driver_error == UINT32_MAX)
    {
        app_wifi_diagnostics.first_driver_error = (uint32_t)status;
        app_wifi_diagnostics.first_driver_error_function = function_name;
    }
    app_wifi_diagnostics.last_driver_error_function = function_name;
    app_wifi_diagnostics.last_driver_error = (uint32_t)status;
    ++app_wifi_diagnostics.error_count;
}

static bool APP_WiFiStatusFailed(W6X_Status_t status)
{
    if (status != W6X_STATUS_OK)
    {
        ++app_wifi_diagnostics.error_count;
        app_wifi_diagnostics.state = APP_WIFI_STATE_ERROR;
        return true;
    }
    return false;
}

void APP_WiFiTask(void)
{
    static W6X_App_Cb_t callbacks = {
        .APP_wifi_cb = APP_WiFiEventCallback,
        .APP_net_cb = APP_NetEventCallback,
        .APP_mqtt_cb = NULL,
        .APP_ble_cb = APP_BleEventCallback,
        .APP_error_cb = APP_WiFiDriverErrorCallback
    };
    bool provisioning_ready = false;
    bool radio_ready = false;
    bool had_ip = false;
    TickType_t signal_tick = 0U;

    app_wifi_diagnostics.bsp_status = BSP_WIFI_Init();
    app_wifi_diagnostics.powered = BSP_WIFI_IsPowered();
    app_wifi_diagnostics.enabled = BSP_WIFI_IsEnabled();
    app_wifi_diagnostics.ready = BSP_WIFI_IsReady();
    if (app_wifi_diagnostics.bsp_status != BSP_WIFI_OK)
    {
        ++app_wifi_diagnostics.error_count;
        app_wifi_diagnostics.state = APP_WIFI_STATE_ERROR;
        goto idle;
    }
    app_wifi_diagnostics.state = APP_WIFI_STATE_POWERED;

    app_wifi_diagnostics.core_status = (uint32_t)W6X_Init();
    if (APP_WiFiStatusFailed((W6X_Status_t)app_wifi_diagnostics.core_status))
    {
        goto idle;
    }
    app_wifi_diagnostics.state = APP_WIFI_STATE_CORE_READY;
    app_wifi_diagnostics.enabled = BSP_WIFI_IsEnabled();
    app_wifi_diagnostics.ready = BSP_WIFI_IsReady();

    app_wifi_diagnostics.callback_status =
        (uint32_t)W6X_RegisterAppCb(&callbacks);
    if (APP_WiFiStatusFailed(
            (W6X_Status_t)app_wifi_diagnostics.callback_status))
    {
        goto idle;
    }

    const W6X_ModuleInfo_t *module_info = W6X_GetModuleInfo();
    if (module_info != NULL)
    {
        memcpy((void *)app_wifi_diagnostics.module_mac,
               module_info->Mac_Address,
               sizeof(app_wifi_diagnostics.module_mac));
        memcpy((void *)app_wifi_diagnostics.module_sdk_version,
               &module_info->SDK_Version,
               sizeof(app_wifi_diagnostics.module_sdk_version));
        memcpy((void *)app_wifi_diagnostics.module_at_version,
               &module_info->AT_Version,
               sizeof(app_wifi_diagnostics.module_at_version));
        memcpy((void *)app_wifi_diagnostics.module_build_date,
               module_info->Build_Date,
               sizeof(app_wifi_diagnostics.module_build_date));
        memcpy((void *)app_wifi_diagnostics.module_name,
               module_info->ModuleID.ModuleName,
               sizeof(app_wifi_diagnostics.module_name));
        app_wifi_diagnostics.module_id =
            (uint32_t)module_info->ModuleID.ModuleID;
    }

    app_wifi_diagnostics.wifi_init_status = (uint32_t)W6X_WiFi_Init();
    if (APP_WiFiStatusFailed(
            (W6X_Status_t)app_wifi_diagnostics.wifi_init_status))
    {
        goto idle;
    }

    /* Same order as ST's BLE commissioning example: WiFi -> Net -> BLE.
     * Without it W6X_Net_* run on a NULL driver object (asserts are compiled
     * out), which faulted in the AT layer when reading the station IP. */
    app_wifi_diagnostics.net_init_status = (uint32_t)W6X_Net_Init();
    if (APP_WiFiStatusFailed(
            (W6X_Status_t)app_wifi_diagnostics.net_init_status))
    {
        goto idle;
    }
    radio_ready = true; /* W6X Wi-Fi station queries are now safe */

    app_wifi_diagnostics.ble_init_status = (uint32_t)W6X_Ble_Init(
        W6X_BLE_MODE_SERVER, app_wifi_ble_rx_buffer,
        sizeof(app_wifi_ble_rx_buffer));
    if (APP_WiFiStatusFailed(
            (W6X_Status_t)app_wifi_diagnostics.ble_init_status))
    {
        goto idle;
    }
    app_wifi_diagnostics.state = APP_WIFI_STATE_RADIOS_READY;

    app_wifi_diagnostics.provision_init_status = (uint32_t)APP_ProvisionInit();
    if (APP_WiFiStatusFailed((W6X_Status_t)app_wifi_diagnostics.provision_init_status)) { goto idle; }
    provisioning_ready = true;
    /* W6X_Net_Init allocates its context and semaphores from the RTOS heap. */
    app_wifi_diagnostics.heap_min_free = (uint32_t)xPortGetMinimumEverFreeHeapSize();

    app_wifi_diagnostics.ble_address_status =
        (uint32_t)W6X_Ble_GetBDAddress((uint8_t *)app_wifi_diagnostics.ble_address);
    app_wifi_diagnostics.ble_adv_status = (uint32_t)W6X_Ble_AdvStart();
    if (APP_WiFiStatusFailed(
            (W6X_Status_t)app_wifi_diagnostics.ble_adv_status))
    {
        goto idle;
    }
    app_wifi_diagnostics.state = APP_WIFI_STATE_BLE_ADVERTISING;

idle:
    for (;;)
    {
        app_wifi_diagnostics.powered = BSP_WIFI_IsPowered();
        app_wifi_diagnostics.enabled = BSP_WIFI_IsEnabled();
        app_wifi_diagnostics.ready = BSP_WIFI_IsReady();

        if (provisioning_ready) { APP_ProvisionPoll(); }

        /* No IP: crossed icon immediately, no AT traffic. With IP: RSSI poll,
         * and once right away when the address is obtained. */
        const bool has_ip = app_wifi_diagnostics.wifi_has_ip;
        if (!has_ip)
        {
            if (app_wifi_signal_level != 0U)
            {
                app_wifi_diagnostics.wifi_rssi = INT32_MIN;
                APP_WiFiSetSignalLevel(0U);
            }
        }
        else if (radio_ready &&
                 (!had_ip || (TickType_t)(xTaskGetTickCount() - signal_tick) >=
                                 pdMS_TO_TICKS(APP_WIFI_SIGNAL_POLL_MS)))
        {
            signal_tick = xTaskGetTickCount();
            APP_WiFiUpdateSignal();
        }
        had_ip = has_ip;
        osDelay(10U);
    }
}
