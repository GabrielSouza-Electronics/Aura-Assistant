#include "app.h"
#include "bsp_wifi.h"
#include "app_provisioning.h"
#include "app_ui_settings.h"
#include "app_calendar.h"

#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"
#include "w6x_api.h"

#define APP_WIFI_BLE_RX_BUFFER_SIZE 256U

/* Display signal bars. UI policy, not radio limits: common RSSI buckets. */
#define APP_WIFI_SIGNAL_POLL_MS 3000U
#define APP_WIFI_RSSI_3_BARS (-60)
#define APP_WIFI_RSSI_2_BARS (-70)

static uint8_t app_wifi_ble_rx_buffer[APP_WIFI_BLE_RX_BUFFER_SIZE];
static volatile uint8_t app_wifi_signal_level;
/* Written by the W6X event callback, read by APP_WiFiTask. */
static volatile bool app_wifi_connected;
static volatile bool app_wifi_has_ip;

uint8_t APP_WiFi_GetSignalLevel(void)
{
    return app_wifi_signal_level;
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
        APP_UISettings_SetText(APP_UI_SETTING_WIFI, "DISCONNECTED");
        app_wifi_signal_level = 0U;
        return;
    }
    info.SSID[sizeof(info.SSID) - 1U] = '\0';
    APP_UISettings_SetText(APP_UI_SETTING_WIFI, (const char *)info.SSID);
    app_wifi_signal_level = (state != W6X_WIFI_STATE_STA_GOT_IP) ? 0U :
                            (info.Rssi >= APP_WIFI_RSSI_3_BARS) ? 3U :
                            (info.Rssi >= APP_WIFI_RSSI_2_BARS) ? 2U : 1U;
}

static void APP_WiFiEventCallback(W6X_event_id_t event_id, void *event_args)
{
    (void)event_args;
    APP_ProvisionWiFiEvent();

    switch (event_id)
    {
        case W6X_WIFI_EVT_CONNECTED_ID:
            app_wifi_connected = true;
            break;
        case W6X_WIFI_EVT_GOT_IP_ID:
            app_wifi_has_ip = true;
            break;
        case W6X_WIFI_EVT_DISCONNECTED_ID:
            app_wifi_connected = false;
            app_wifi_has_ip = false;
            break;
        default:
            break;
    }
}

/* W6X_Net_Init refuses to start without a registered Net callback. Sockets are
 * not used yet. */
static void APP_NetEventCallback(W6X_event_id_t event_id, void *event_args)
{
    (void)event_id;
    (void)event_args;
}

static void APP_BleEventCallback(W6X_event_id_t event_id, void *event_args)
{
    APP_ProvisionBleEvent(event_id, event_args, app_wifi_ble_rx_buffer,
                          sizeof(app_wifi_ble_rx_buffer));
}

void APP_WiFiTask(void)
{
    static W6X_App_Cb_t callbacks = {
        .APP_wifi_cb = APP_WiFiEventCallback,
        .APP_net_cb = APP_NetEventCallback,
        .APP_mqtt_cb = NULL,
        .APP_ble_cb = APP_BleEventCallback,
        .APP_error_cb = NULL
    };
    bool provisioning_ready = false;
    bool radio_ready = false;
    bool had_ip = false;
    TickType_t signal_tick = 0U;
    bool had_connection = false;
    bool ble_shown = false;

    /* Any failed step leaves the radio idle; the UI keeps showing no IP. */
    if ((BSP_WIFI_Init() != BSP_WIFI_OK) ||
        (W6X_Init() != W6X_STATUS_OK) ||
        (W6X_RegisterAppCb(&callbacks) != W6X_STATUS_OK) ||
        (W6X_WiFi_Init() != W6X_STATUS_OK) ||
        /* Same order as ST's BLE commissioning example: WiFi -> Net -> BLE.
         * Without it W6X_Net_* run on a NULL driver object (asserts are
         * compiled out), which faulted in the AT layer reading the IP. */
        (W6X_Net_Init() != W6X_STATUS_OK))
    {
        goto idle;
    }
    radio_ready = true; /* W6X Wi-Fi station queries are now safe */
    (void)APP_CalendarStart();

    if ((W6X_Ble_Init(W6X_BLE_MODE_SERVER, app_wifi_ble_rx_buffer,
                      sizeof(app_wifi_ble_rx_buffer)) != W6X_STATUS_OK) ||
        (APP_ProvisionInit() != W6X_STATUS_OK))
    {
        goto idle;
    }
    /* Settings ON starts advertising and the provisioning window. */
    provisioning_ready = true;

idle:
    for (;;)
    {
        if (provisioning_ready) { APP_ProvisionPoll(); }
        const bool ble_enabled = APP_ProvisionIsEnabled();
        if (ble_enabled != ble_shown)
        {
            ble_shown = ble_enabled;
            APP_UISettings_SetText(APP_UI_SETTING_BLUETOOTH,
                                   ble_enabled ? "ON" : "OFF");
        }

        /* Publish the associated SSID even while DHCP is in progress;
         * refresh on association/IP acquisition and then every 3 seconds. */
        const bool has_ip = app_wifi_has_ip;
        APP_CalendarSetOnline(radio_ready && has_ip);
        const bool connected = app_wifi_connected || has_ip;
        if (!connected && had_connection)
        { APP_UISettings_SetText(APP_UI_SETTING_WIFI, "DISCONNECTED"); }
        if (!has_ip)
        {
            app_wifi_signal_level = 0U;
        }
        if (connected && radio_ready &&
                 (!had_connection || (has_ip && !had_ip) ||
                  (TickType_t)(xTaskGetTickCount() - signal_tick) >=
                                 pdMS_TO_TICKS(APP_WIFI_SIGNAL_POLL_MS)))
        {
            signal_tick = xTaskGetTickCount();
            APP_WiFiUpdateSignal();
        }
        had_ip = has_ip;
        had_connection = connected;
        osDelay(10U);
    }
}
