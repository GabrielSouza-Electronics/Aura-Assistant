#include "app.h"

#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "spi_iface.h"
#include "w6x_api.h"

#include <limits.h>
#include <string.h>

#define APP_WIFI_BLE_RX_BUFFER_SIZE 256U

volatile APP_WiFiDiagnostics_t app_wifi_diagnostics = {
    .magic = 0x57494649U, /* "WIFI" */
    .state = APP_WIFI_STATE_OFF,
    .wifi_best_rssi = INT32_MIN
};

static uint8_t app_wifi_ble_rx_buffer[APP_WIFI_BLE_RX_BUFFER_SIZE];
static volatile bool app_wifi_restart_advertising;

static void APP_WiFiEventCallback(W6X_event_id_t event_id, void *event_args)
{
    (void)event_args;
    app_wifi_diagnostics.last_wifi_event = event_id;

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

static void APP_BleEventCallback(W6X_event_id_t event_id, void *event_args)
{
    (void)event_args;
    app_wifi_diagnostics.last_ble_event = event_id;

    if (event_id == W6X_BLE_EVT_CONNECTED_ID)
    {
        app_wifi_diagnostics.ble_connected = true;
        ++app_wifi_diagnostics.ble_connection_count;
    }
    else if (event_id == W6X_BLE_EVT_DISCONNECTED_ID)
    {
        app_wifi_diagnostics.ble_connected = false;
        app_wifi_restart_advertising = true;
    }
}

static void APP_WiFiDriverErrorCallback(W6X_Status_t status,
                                        char const *function_name)
{
    (void)function_name;
    app_wifi_diagnostics.last_driver_error = (uint32_t)status;
    ++app_wifi_diagnostics.error_count;
}

static void APP_WiFiScanCallback(int32_t status,
                                 W6X_WiFi_Scan_Result_t *results)
{
    app_wifi_diagnostics.wifi_scan_callback_status = (uint32_t)status;
    if (results == NULL)
    {
        ++app_wifi_diagnostics.error_count;
        return;
    }

    app_wifi_diagnostics.wifi_ap_count = results->Count;
    app_wifi_diagnostics.wifi_best_rssi = INT32_MIN;
    for (uint32_t index = 0U; index < results->Count; ++index)
    {
        if (results->AP[index].RSSI > app_wifi_diagnostics.wifi_best_rssi)
        {
            app_wifi_diagnostics.wifi_best_rssi = results->AP[index].RSSI;
        }
    }
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
        .APP_net_cb = NULL,
        .APP_mqtt_cb = NULL,
        .APP_ble_cb = APP_BleEventCallback,
        .APP_error_cb = APP_WiFiDriverErrorCallback
    };
    W6X_WiFi_Scan_Opts_t scan_options = {0};

    app_wifi_diagnostics.bsp_status = BSP_WIFI_Init();
    app_wifi_diagnostics.spi_state_before_init =
        BSP_WIFI_GetSPIStateBeforeInit();
    app_wifi_diagnostics.spi_state_after_init =
        BSP_WIFI_GetSPIStateAfterInit();
    app_wifi_diagnostics.spi_error = BSP_WIFI_GetSPIError();
    app_wifi_diagnostics.spi_reinit_count = BSP_WIFI_GetSPIReinitCount();
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

    app_wifi_diagnostics.heap_free_before_core = xPortGetFreeHeapSize();
    app_wifi_diagnostics.core_status = (uint32_t)W6X_Init();
    app_wifi_diagnostics.heap_free_after_core = xPortGetFreeHeapSize();
    app_wifi_diagnostics.heap_minimum_ever_free =
        xPortGetMinimumEverFreeHeapSize();
    if (APP_WiFiStatusFailed((W6X_Status_t)app_wifi_diagnostics.core_status))
    {
        /* Diagnostic only: if RDY stayed high, re-submit the event once after
         * the modem-ready timeout.  A resulting SPI transfer proves that the
         * engine task is alive and that the original RDY event was missed. */
        if (BSP_WIFI_IsReady())
        {
            ++app_wifi_diagnostics.spi_forced_ready_kick_count;
            (void)spi_on_txn_data_ready();
            osDelay(100U);
        }
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

    app_wifi_diagnostics.ble_init_status = (uint32_t)W6X_Ble_Init(
        W6X_BLE_MODE_SERVER, app_wifi_ble_rx_buffer,
        sizeof(app_wifi_ble_rx_buffer));
    if (APP_WiFiStatusFailed(
            (W6X_Status_t)app_wifi_diagnostics.ble_init_status))
    {
        goto idle;
    }
    app_wifi_diagnostics.state = APP_WIFI_STATE_RADIOS_READY;

    (void)W6X_Ble_GetBDAddress((uint8_t *)app_wifi_diagnostics.ble_address);
    app_wifi_diagnostics.ble_adv_status = (uint32_t)W6X_Ble_AdvStart();
    if (APP_WiFiStatusFailed(
            (W6X_Status_t)app_wifi_diagnostics.ble_adv_status))
    {
        goto idle;
    }
    app_wifi_diagnostics.state = APP_WIFI_STATE_BLE_ADVERTISING;

    scan_options.MaxCnt = 10U;
    app_wifi_diagnostics.wifi_scan_status = (uint32_t)W6X_WiFi_Scan(
        &scan_options, APP_WiFiScanCallback);
    if ((W6X_Status_t)app_wifi_diagnostics.wifi_scan_status != W6X_STATUS_OK)
    {
        ++app_wifi_diagnostics.error_count;
    }

idle:
    for (;;)
    {
        app_wifi_diagnostics.powered = BSP_WIFI_IsPowered();
        app_wifi_diagnostics.enabled = BSP_WIFI_IsEnabled();
        app_wifi_diagnostics.ready = BSP_WIFI_IsReady();
        app_wifi_diagnostics.ready_irq_count = BSP_WIFI_GetReadyIRQCount();
        app_wifi_diagnostics.spi_transfer_count =
            BSP_WIFI_GetSPITransferCount();
        app_wifi_diagnostics.spi_last_hal_status =
            BSP_WIFI_GetSPILastHALStatus();
        app_wifi_diagnostics.spi_last_length = BSP_WIFI_GetSPILastLength();
        app_wifi_diagnostics.spi_cs_assert_count =
            BSP_WIFI_GetCSAssertCount();
        app_wifi_diagnostics.spi_cs_deassert_count =
            BSP_WIFI_GetCSDeassertCount();
        app_wifi_diagnostics.spi_engine_task_create_status =
            BSP_WIFI_GetSPIEngineTaskCreateStatus();
        app_wifi_diagnostics.spi_engine_task_start_count =
            BSP_WIFI_GetSPIEngineTaskStartCount();
        app_wifi_diagnostics.spi_engine_task_wake_count =
            BSP_WIFI_GetSPIEngineTaskWakeCount();
        app_wifi_diagnostics.spi_engine_last_event_bits =
            BSP_WIFI_GetSPIEngineLastEventBits();
        app_wifi_diagnostics.spi_engine_deinit_count =
            BSP_WIFI_GetSPIEngineDeinitCount();
        spi_get_bringup_diagnostics(
            (uint32_t *)&app_wifi_diagnostics.spi_engine_task_present,
            (uint32_t *)&app_wifi_diagnostics.spi_engine_task_start_count,
            (uint32_t *)&app_wifi_diagnostics.spi_engine_task_wake_count,
            (uint32_t *)&app_wifi_diagnostics.spi_engine_last_event_bits,
            (uint32_t *)&app_wifi_diagnostics.spi_engine_initialized,
            (uint32_t *)&app_wifi_diagnostics.spi_engine_init_stage,
            (uint32_t *)&app_wifi_diagnostics.spi_engine_task_handle);
        BSP_WIFI_GetSPILastRX((uint8_t *)app_wifi_diagnostics.spi_last_rx);

        if (app_wifi_restart_advertising)
        {
            app_wifi_restart_advertising = false;
            app_wifi_diagnostics.ble_adv_status =
                (uint32_t)W6X_Ble_AdvStart();
            if ((W6X_Status_t)app_wifi_diagnostics.ble_adv_status !=
                W6X_STATUS_OK)
            {
                ++app_wifi_diagnostics.error_count;
            }
        }
        osDelay(100U);
    }
}
