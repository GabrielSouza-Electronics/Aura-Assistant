#include "app_provisioning.h"
#include "provision_protocol.h"
#include "FreeRTOS.h"
#include "task.h"
#include "w61_driver_config.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define SERVICE_INDEX 0U
#define COMMAND_INDEX 0U
#define EVENT_INDEX 1U
#define RX_SLOTS 48U
#define SCAN_MAX 10U
/* Application policy, not hardware timings. Settings opens a new window. */
#ifndef AURA_PROV_DEV_TIMING
#ifdef DEBUG
/* Debug builds only: allow hand-typed nRF Connect writes and debugger pauses. */
#define AURA_PROV_DEV_TIMING 1
#else
#define AURA_PROV_DEV_TIMING 0
#endif
#endif
#if AURA_PROV_DEV_TIMING
#define FRAME_MS 120000U
#else
#define FRAME_MS 10000U
#endif
#define WINDOW_MS 300000U
#define PAIR_MS 60000U
#define SCAN_MS 30000U
#define SEND_MS 1000U
#define ATT_MTU_DEFAULT 23U
#define ATT_HEADER 3U
#define NOTIFY_MAX 244U /* AT+BLEGATTSNTFY data limit */

/* Advertising payload bisect (AdvStart returned ERROR with mode 2):
 * 0 = module default advertising/scan response data (test A: advertises),
 * 1 = only the 128-bit service UUID, no Flags, no scan response (test B),
 * 2 = Flags + UUID, name in scan response (original, rejected by AdvStart),
 * 3 = UUID only (as 1) + complete name in scan response (test C: rejected
 *     by AdvStart on SDK 2.0.89, so any custom scan response is avoided),
 * 4 = UUID + shortened name "Aura" in the advertising packet (24 of 31 bytes),
 *     no scan response. Full GAP name stays "AuraAssistant" (W6X_Ble_Init). */
#ifndef AURA_PROV_ADV_MODE
#define AURA_PROV_ADV_MODE 4
#endif

#if W61_AT_LOG_ENABLE || MDM_CMD_LOG_ENABLE
#error "Provisioning requires raw AT/modem logging disabled (Wi-Fi credentials)."
#endif

/* CPU-only scratch in SRAM4, explicitly cleared by Init (NOLOAD). */
#define PROV_STORAGE __attribute__((section(".provisioning"), aligned(4)))

typedef struct {
    uint32_t epoch;
    uint8_t length;
    bool paired;
    uint8_t data[PROV_CHUNK_MAX];
} RxChunk;

/* Shared with the ST callback task; all accesses are under a critical section. */
static struct {
    uint32_t epoch;
    uint8_t handle;
    uint8_t reject_handle;
    bool connected, paired, subscribed, confirm, failed, poisoned;
    uint16_t mtu; /* ATT MTU of the current link (23 until an exchange is reported) */
    RxChunk rx[RX_SLOTS];
    unsigned head, tail, count;
} link PROV_STORAGE;

static struct {
    bool pending, done;
    int32_t status;
    uint32_t count;
    W6X_WiFi_Ap_t ap[SCAN_MAX];
} scan PROV_STORAGE;

static ProvFrame frame PROV_STORAGE;
static uint32_t worker_epoch, last_id;
static TickType_t boot_tick, pair_tick, frame_tick;
static bool initialized, window_open, security_started, frame_paired;
static bool announced_paired, announced_subscribed;
static bool wifi_changed;
static bool restart_advertising;
static TickType_t advertising_tick;
static bool enable_request_pending, enable_requested, ui_enabled;
static bool stop_advertising_pending;
static TickType_t stop_advertising_tick;
volatile APP_ProvisionDiagnostics app_provision_diagnostics;

void APP_ProvisionRequestEnabled(bool enabled)
{
    taskENTER_CRITICAL();
    enable_requested = enabled;
    enable_request_pending = true;
    taskEXIT_CRITICAL();
}

bool APP_ProvisionIsEnabled(void)
{
    bool enabled;
    taskENTER_CRITICAL(); enabled = ui_enabled; taskEXIT_CRITICAL();
    return enabled;
}

static void PublishEnabled(bool enabled)
{
    taskENTER_CRITICAL(); ui_enabled = enabled; taskEXIT_CRITICAL();
}

void APP_ProvisionWiFiEvent(void)
{
    taskENTER_CRITICAL(); wifi_changed = true; taskEXIT_CRITICAL();
}

static bool Session(uint32_t epoch, uint8_t *handle, bool require_notify)
{
    bool ok;
    taskENTER_CRITICAL();
    ok = link.connected && link.epoch == epoch && !link.poisoned &&
         (!require_notify || link.subscribed);
    *handle = link.handle;
    taskEXIT_CRITICAL();
    return ok;
}

static bool Paired(void)
{
    bool paired;
    taskENTER_CRITICAL();
    paired = link.connected && link.epoch == worker_epoch && link.paired;
    taskEXIT_CRITICAL();
    return paired;
}

static bool Window(void)
{
    if (window_open && (TickType_t)(xTaskGetTickCount() - boot_tick) >= pdMS_TO_TICKS(WINDOW_MS))
    { window_open = false; }
    app_provision_diagnostics.window_open = window_open;
    return window_open;
}

static bool Send(uint32_t epoch, const char *message)
{
    size_t length = strlen(message);
    for (size_t offset = 0U; offset < length;)
    {
        uint8_t handle;
        uint32_t sent = 0U;
        uint32_t count = (uint32_t)(length - offset);
        /* Writes stay limited to PROV_CHUNK_MAX by the v1 contract; notifications
         * may use the negotiated MTU (clients reassemble up to LF). Fewer AT
         * notify transactions per message. */
        uint32_t max_chunk = PROV_CHUNK_MAX;
        taskENTER_CRITICAL();
        if (link.mtu > ATT_MTU_DEFAULT) { max_chunk = (uint32_t)link.mtu - ATT_HEADER; }
        taskEXIT_CRITICAL();
        if (max_chunk > NOTIFY_MAX) { max_chunk = NOTIFY_MAX; }
        app_provision_diagnostics.tx_chunk = max_chunk;
        if (count > max_chunk) { count = max_chunk; }
        if (!Session(epoch, &handle, true)) { return false; }
        W6X_Status_t status = W6X_Ble_ServerNotify(handle, SERVICE_INDEX, EVENT_INDEX,
            (uint8_t *)(message + offset), count, &sent, SEND_MS);
        if (status != W6X_STATUS_OK || sent != count)
        {
            ++app_provision_diagnostics.tx_failures;
            /* A partial JSON line must never be joined to another message. */
            taskENTER_CRITICAL();
            if (link.epoch == epoch) { link.poisoned = true; }
            taskEXIT_CRITICAL();
            return false;
        }
        offset += count;
    }
    return true;
}

static bool Event(uint32_t id, const char *event)
{
    char message[96];
    (void)snprintf(message, sizeof(message),
                   "{\"v\":1,\"id\":%" PRIu32 ",\"event\":\"%s\"}\n", id, event);
    return Send(worker_epoch, message);
}

static void Error(uint32_t id, const char *code)
{
    char message[128];
    ++app_provision_diagnostics.rejected;
    app_provision_diagnostics.last_error = code; /* string literals only */
    (void)snprintf(message, sizeof(message),
                   "{\"v\":1,\"id\":%" PRIu32 ",\"event\":\"error\",\"code\":\"%s\"}\n", id, code);
    (void)Send(worker_epoch, message);
}

static const char *Security(W6X_WiFi_SecurityType_e security)
{
    switch (security)
    {
        case W6X_WIFI_SECURITY_OPEN: return "open";
        case W6X_WIFI_SECURITY_WPA_PSK: return "wpa";
        case W6X_WIFI_SECURITY_WPA2_PSK: return "wpa2";
        case W6X_WIFI_SECURITY_WPA_WPA2_PSK: return "wpa_wpa2";
        case W6X_WIFI_SECURITY_WPA3_SAE: return "wpa3";
        case W6X_WIFI_SECURITY_WPA2_WPA3_SAE: return "wpa2_wpa3";
        case W6X_WIFI_SECURITY_WEP: return "wep";
        case W6X_WIFI_SECURITY_WPA_ENT: return "enterprise";
        default: return "unknown";
    }
}

static bool Status(uint32_t id, bool connected_event)
{
    char message[512], ssid[PROV_SSID_MAX * 6U + 3U];
    uint8_t ip[4] = {0}, gateway[4], netmask[4];
    W6X_WiFi_Connect_t info = {0};
    W6X_WiFi_StaStateType_e state;
    bool paired = Paired();
    if (W6X_WiFi_Station_GetState(&state, paired ? &info : NULL) != W6X_STATUS_OK)
    { Error(id, "status_failed"); return false; }
    bool has_ip = state == W6X_WIFI_STATE_STA_GOT_IP;
    if (has_ip && paired)
    {
        if (W6X_Net_Station_GetIPAddress(ip, gateway, netmask) != W6X_STATUS_OK ||
            (ip[0] | ip[1] | ip[2] | ip[3]) == 0U)
        { Error(id, "ip_unavailable"); return false; }
    }
    if (connected_event && !has_ip) { Error(id, "connection_failed"); return false; }
    info.SSID[PROV_SSID_MAX] = 0U;
    if (!Prov_Quote((const char *)info.SSID, ssid, sizeof(ssid))) { strcpy(ssid, "\"\""); }
    const char *wifi = has_ip ? "connected" : (state == W6X_WIFI_STATE_STA_CONNECTING ?
                      "connecting" : (state == W6X_WIFI_STATE_STA_CONNECTED ? "associated" : "disconnected"));
    int size = snprintf(message, sizeof(message),
        "{\"v\":1,\"id\":%" PRIu32 ",\"event\":\"%s\",\"wifi\":\"%s\","
        "\"ssid\":%s,\"ip\":\"%u.%u.%u.%u\",\"security\":\"%s\","
        "\"provisioning_open\":%s,\"max_chunk\":20,\"max_message\":768}\n",
        id, connected_event ? "connected" : "status", wifi, ssid,
        ip[0], ip[1], ip[2], ip[3], paired ? "ready" : "pairing", Window() ? "true" : "false");
    return size > 0 && (size_t)size < sizeof(message) && Send(worker_epoch, message);
}

void APP_ProvisionScanResult(int32_t status, W6X_WiFi_Scan_Result_t *results)
{
    taskENTER_CRITICAL();
    if (scan.pending)
    {
        scan.status = status;
        scan.count = 0U;
        if (results == NULL || (results->Count != 0U && results->AP == NULL)) { scan.status = -1; }
        else
        {
            scan.count = results->Count > SCAN_MAX ? SCAN_MAX : results->Count;
            if (scan.count != 0U) { memcpy(scan.ap, results->AP, scan.count * sizeof(scan.ap[0])); }
        }
        scan.done = true;
    }
    taskEXIT_CRITICAL();
}

static void Scan(uint32_t id)
{
    bool pending;
    taskENTER_CRITICAL();
    /* A timed-out scan still owns the ST callback until it completes. */
    if (scan.pending && scan.done) { scan.pending = false; }
    pending = scan.pending;
    if (!pending) { scan.pending = true; scan.done = false; }
    taskEXIT_CRITICAL();
    if (pending) { Error(id, "scan_busy"); return; }
    app_provision_diagnostics.scan_stage = "notify_started";
    if (!Event(id, "scan_started"))
    {
        app_provision_diagnostics.scan_stage = "notify_started_failed";
        taskENTER_CRITICAL(); scan.pending = false; taskEXIT_CRITICAL(); return;
    }
    W6X_WiFi_Scan_Opts_t options = {0};
    options.MaxCnt = SCAN_MAX;
    app_provision_diagnostics.scan_stage = "calling_wifi_scan";
    W6X_Status_t result = W6X_WiFi_Scan(&options, APP_ProvisionScanResult);
    if (result != W6X_STATUS_OK)
    {
        /* Keep ownership: an AT timeout can still deliver a late callback. */
        app_provision_diagnostics.scan_stage = "wifi_scan_failed";
        Error(id, "scan_failed"); return;
    }
    app_provision_diagnostics.scan_stage = "waiting_results";
    TickType_t start = xTaskGetTickCount();
    for (;;)
    {
        bool done;
        uint8_t handle;
        taskENTER_CRITICAL(); done = scan.done; taskEXIT_CRITICAL();
        if (done) { break; }
        if (!Session(worker_epoch, &handle, true))
        { app_provision_diagnostics.scan_stage = "client_lost"; return; }
        if ((TickType_t)(xTaskGetTickCount() - start) >= pdMS_TO_TICKS(SCAN_MS))
        { app_provision_diagnostics.scan_stage = "timeout"; Error(id, "scan_timeout"); return; }
        vTaskDelay(pdMS_TO_TICKS(10U));
    }
    /* No new scan can start while this task serializes the stable snapshot. */
    if (scan.status != 0) { app_provision_diagnostics.scan_stage = "results_failed"; Error(id, "scan_failed"); }
    else
    {
        app_provision_diagnostics.scan_stage = "sending_results";
        uint32_t sent = 0U, skipped = 0U;
        for (uint32_t i = 0U; i < scan.count; ++i)
        {
            W6X_WiFi_Ap_t *ap = &scan.ap[i];
            char quoted[PROV_SSID_MAX * 6U + 3U], message[384];
            ap->SSID[PROV_SSID_MAX] = 0U;
            if (!Prov_Quote((const char *)ap->SSID, quoted, sizeof(quoted))) { ++skipped; continue; }
            bool supported = ap->Security != W6X_WIFI_SECURITY_WEP &&
                             ap->Security != W6X_WIFI_SECURITY_WPA_ENT && ap->Security != W6X_WIFI_SECURITY_UNKNOWN;
            int n = snprintf(message, sizeof(message),
                "{\"v\":1,\"id\":%" PRIu32 ",\"event\":\"network\",\"ssid\":%s,\"rssi\":%d,"
                "\"security\":\"%s\",\"channel\":%u,\"supported\":%s,"
                "\"bssid\":\"%02x:%02x:%02x:%02x:%02x:%02x\"}\n",
                id, quoted, ap->RSSI, Security(ap->Security), ap->Channel, supported ? "true" : "false",
                ap->MAC[0], ap->MAC[1], ap->MAC[2], ap->MAC[3], ap->MAC[4], ap->MAC[5]);
            if (n <= 0 || (size_t)n >= sizeof(message) || !Send(worker_epoch, message)) { break; }
            ++sent;
        }
        char message[144];
        (void)snprintf(message, sizeof(message),
            "{\"v\":1,\"id\":%" PRIu32 ",\"event\":\"scan_done\",\"count\":%" PRIu32
            ",\"limit\":10,\"skipped\":%" PRIu32 "}\n", id, sent, skipped);
        app_provision_diagnostics.scan_stage = Send(worker_epoch, message) ? "done" : "send_failed";
    }
    taskENTER_CRITICAL(); scan.pending = false; taskEXIT_CRITICAL();
}

static void Connect(const ProvCommand *command)
{
    W6X_WiFi_Connect_Opts_t options = {0};
    W6X_WiFi_StaStateType_e state;
    if (!Event(command->id, "connecting")) { return; }
    if (W6X_WiFi_Station_GetState(&state, NULL) != W6X_STATUS_OK)
    { Error(command->id, "status_failed"); return; }
    if (state == W6X_WIFI_STATE_STA_CONNECTED || state == W6X_WIFI_STATE_STA_GOT_IP ||
        state == W6X_WIFI_STATE_STA_CONNECTING)
    {
        if (W6X_WiFi_Disconnect(0U) != W6X_STATUS_OK)
        { Error(command->id, "disconnect_failed"); return; }
    }
    memcpy(options.SSID, command->ssid, sizeof(options.SSID));
    memcpy(options.Password, command->password, sizeof(options.Password));
    options.Reconnection_interval = 5U;
    options.Reconnection_nb_attempts = 3U;
    W6X_Status_t result = W6X_WiFi_Connect(&options);
    Prov_Clear(&options, sizeof(options));
    app_provision_diagnostics.connect_status = (uint32_t)result;
    if (result == W6X_STATUS_OK) { (void)Status(command->id, true); }
    else
    {
        /* Stop late retries before accepting another set of credentials. */
        (void)W6X_WiFi_Disconnect(0U);
        Error(command->id, "connection_failed");
    }
}

static void Forget(uint32_t id)
{
    W6X_WiFi_CredentialsList_t credentials = {0};
    W6X_Status_t result = W6X_WiFi_GetCredentials(&credentials);
    if (result != W6X_STATUS_OK || credentials.Count > W6X_WIFI_MAX_SSID_LIST_SIZE)
    { Error(id, "forget_failed"); return; }
    /* restore=1 clears the last connection and disables its next-boot restore. */
    result = W6X_WiFi_Disconnect(1U);
    bool ok = result == W6X_STATUS_OK;
    for (uint32_t i = 0U; i < credentials.Count; ++i)
    {
        credentials.SSID[i][PROV_SSID_MAX] = 0U;
        /* Some NCP versions may have removed the active entry with restore=1.
         * Verify the final store rather than treating "already absent" as fatal. */
        (void)W6X_WiFi_DeleteCredentials(credentials.SSID[i]);
    }
    memset(&credentials, 0, sizeof(credentials));
    if (W6X_WiFi_GetCredentials(&credentials) != W6X_STATUS_OK || credentials.Count != 0U) { ok = false; }
    if (ok) { (void)Event(id, "forgotten"); }
    else { Error(id, "forget_failed"); }
}

static void Dispatch(bool received_paired)
{
    ProvCommand command;
    const char *error = Prov_Parse(frame.data, &command);
    Prov_FrameReset(&frame);
    if (error != NULL) { Error(0U, error); }
    else if (command.id <= last_id) { Error(command.id, "stale_id"); }
    else
    {
        last_id = command.id;
        app_provision_diagnostics.last_request_id = last_id;
        ++app_provision_diagnostics.commands;
        app_provision_diagnostics.last_command =
            command.kind == PROV_STATUS ? "get_status" : command.kind == PROV_SCAN ? "scan" :
            command.kind == PROV_CONNECT ? "connect" : "forget";
        if (command.kind == PROV_STATUS) { (void)Status(command.id, false); }
        else if (!received_paired || !Paired()) { Error(command.id, "pairing_required"); }
        else if (!Window()) { Error(command.id, "provisioning_closed"); }
        else
        {
            switch (command.kind)
            {
                case PROV_SCAN: Scan(command.id); break;
                case PROV_CONNECT: Connect(&command); break;
                case PROV_FORGET: Forget(command.id); break;
                default: break;
            }
        }
    }
    Prov_Clear(&command, sizeof(command));
}

void APP_ProvisionBleEvent(W6X_event_id_t event, void *args, uint8_t *data, size_t capacity)
{
    const W6X_Ble_CbParamData_t *p = args;
    if (!initialized || p == NULL) { return; }
    uint8_t handle = p->remote_ble_device.conn_handle;
    if (event >= W6X_BLE_EVT_PAIRING_FAILED_ID && event <= W6X_BLE_EVT_PAIRING_CANCELED_ID)
    {
        app_provision_diagnostics.last_security_event = (uint32_t)event;
        app_provision_diagnostics.last_security_handle = handle;
    }
    taskENTER_CRITICAL();
    if (event == W6X_BLE_EVT_CONNECTED_ID)
    {
        if (link.connected)
        {
            /* Older NCPs do not address notifications to a specific handle.
             * Fail closed if a second client races the advertising stop. */
            link.reject_handle = handle;
            link.poisoned = true;
        }
        else
        {
            ++link.epoch;
            link.handle = handle;
            link.connected = true;
            link.mtu = ATT_MTU_DEFAULT;
            link.paired = link.subscribed = link.confirm = link.failed = link.poisoned = false;
            Prov_Clear(link.rx, sizeof(link.rx));
            link.head = link.tail = link.count = 0U;
        }
    }
    else if (event == W6X_BLE_EVT_NOTIFICATION_STATUS_ENABLED_ID ||
             event == W6X_BLE_EVT_NOTIFICATION_STATUS_DISABLED_ID)
    {
        /* ST's CCCD event does not contain a connection handle. Only one client
           is admitted and advertising is stopped after its connection. */
        if (link.connected && p->service_idx == SERVICE_INDEX && p->charac_idx == EVENT_INDEX)
        { link.subscribed = event == W6X_BLE_EVT_NOTIFICATION_STATUS_ENABLED_ID; }
    }
    else if (link.connected &&
             (handle == link.handle ||
              /* The ST driver resolves pairing events by BD address. Phones connect
               * with a resolvable private address but report their identity address
               * at pairing, so the lookup yields 0xFF. Only one client is admitted
               * (as for CCCD events above), so an unresolved security event belongs
               * to the current link. */
              (handle == UINT8_MAX &&
               (event == W6X_BLE_EVT_PAIRING_COMPLETED_ID || event == W6X_BLE_EVT_PAIRING_CONFIRM_ID ||
                event == W6X_BLE_EVT_PAIRING_FAILED_ID || event == W6X_BLE_EVT_PAIRING_CANCELED_ID))))
    {
        switch (event)
        {
            case W6X_BLE_EVT_DISCONNECTED_ID:
                ++link.epoch;
                link.connected = link.paired = link.subscribed = false;
                Prov_Clear(link.rx, sizeof(link.rx));
                link.count = link.head = link.tail = 0U;
                break;
            case W6X_BLE_EVT_PAIRING_COMPLETED_ID: link.paired = true; break;
            case W6X_BLE_EVT_MTU_SIZE_ID:
                link.mtu = p->mtu_size;
                app_provision_diagnostics.mtu = p->mtu_size;
                break;
            case W6X_BLE_EVT_PAIRING_CONFIRM_ID: link.confirm = true; break;
            case W6X_BLE_EVT_PAIRING_FAILED_ID:
            case W6X_BLE_EVT_PAIRING_CANCELED_ID: link.paired = false; link.failed = true; break;
            case W6X_BLE_EVT_WRITE_ID:
                if (p->service_idx != SERVICE_INDEX || p->charac_idx != COMMAND_INDEX) { break; }
                if (!link.subscribed || link.poisoned) { break; }
                if (p->available_data_length == 0U || p->available_data_length > PROV_CHUNK_MAX ||
                    p->available_data_length > capacity || link.count == RX_SLOTS)
                { link.poisoned = true; ++app_provision_diagnostics.rx_overflows; break; }
                RxChunk *chunk = &link.rx[link.head];
                chunk->epoch = link.epoch;
                chunk->paired = link.paired;
                chunk->length = (uint8_t)p->available_data_length;
                memcpy(chunk->data, data, chunk->length);
                link.head = (link.head + 1U) % RX_SLOTS;
                ++link.count;
                break;
            default: break;
        }
    }
    taskEXIT_CRITICAL();
    if (event == W6X_BLE_EVT_WRITE_ID) { Prov_Clear(data, capacity); }
}

static void RefreshBondCount(void);

W6X_Status_t APP_ProvisionInit(void)
{
    /* Advertising UUID is little-endian; name is carried in scan response. */
#if AURA_PROV_ADV_MODE == 1
    static const char advertising[] = "11073412908f6e2c7d9a3c4fa3b501a0577e";
#elif AURA_PROV_ADV_MODE == 4
    /* UUID structure as mode 1, then 0x05 length, 0x08 Shortened Local Name "Aura". */
    static const char advertising[] = "11073412908f6e2c7d9a3c4fa3b501a0577e050841757261";
#elif AURA_PROV_ADV_MODE == 3
    static const char advertising[] = "11073412908f6e2c7d9a3c4fa3b501a0577e";
    /* 0x0e length, 0x09 Complete Local Name, "AuraAssistant" (13 bytes). */
    static const char scan_response[] = "0e0941757261417373697374616e74";
#elif AURA_PROV_ADV_MODE == 2
    static const char advertising[] = "02010611073412908f6e2c7d9a3c4fa3b501a0577e";
    static const char scan_response[] = "0e0941757261417373697374616e74";
#endif
    W6X_Status_t result;
    app_provision_diagnostics.adv_param_status = UINT32_MAX;
    Prov_Clear(&link, sizeof(link));
    Prov_Clear(&scan, sizeof(scan));
    Prov_FrameReset(&frame);
    link.reject_handle = UINT8_MAX;
    boot_tick = xTaskGetTickCount();
    window_open = false;
    PublishEnabled(false);
    initialized = true;
    result = W6X_Ble_CreateService(SERVICE_INDEX, AURA_PROV_SERVICE_UUID, W6X_BLE_UUID_TYPE_128);
    if (result == W6X_STATUS_OK)
    { result = W6X_Ble_CreateCharacteristic(SERVICE_INDEX, COMMAND_INDEX, AURA_PROV_COMMAND_UUID,
        W6X_BLE_UUID_TYPE_128, W6X_BLE_CHAR_PROP_WRITE_WITH_RESP, W6X_BLE_CHAR_PERM_WRITE); }
    if (result == W6X_STATUS_OK)
    { result = W6X_Ble_CreateCharacteristic(SERVICE_INDEX, EVENT_INDEX, AURA_PROV_EVENT_UUID,
        W6X_BLE_UUID_TYPE_128, W6X_BLE_CHAR_PROP_NOTIFY, W6X_BLE_CHAR_PERM_READ); }
    if (result == W6X_STATUS_OK) { result = W6X_Ble_RegisterCharacteristics(); }
    if (result == W6X_STATUS_OK) { result = W6X_Ble_SetSecurityParam(W6X_BLE_SEC_IO_NO_INPUT_OUTPUT); }
    if (result == W6X_STATUS_OK)
    {
        /* ST AT example: 160..320 units of 0.625 ms = 100..200 ms,
         * connectable/scannable on channels 37, 38 and 39. Set before data. */
        result = W6X_Ble_SetAdvParam(160U, 320U, W6X_BLE_ADV_TYPE_IND,
                                    W6X_BLE_ADV_CHANNEL_ALL);
        app_provision_diagnostics.adv_param_status = (uint32_t)result;
    }
#if AURA_PROV_ADV_MODE >= 1
    if (result == W6X_STATUS_OK) { result = W6X_Ble_SetAdvData(advertising); }
#endif
#if AURA_PROV_ADV_MODE == 2 || AURA_PROV_ADV_MODE == 3
    if (result == W6X_STATUS_OK) { result = W6X_Ble_SetScanRespData(scan_response); }
#endif
    app_provision_diagnostics.gatt_status = (uint32_t)result;
    if (result != W6X_STATUS_OK) { initialized = false; }
    else
    {
        /* Does the NCP keep bonds across the power cycle done by BSP_WIFI_Init? */
        RefreshBondCount();
        app_provision_diagnostics.bond_count_at_boot = app_provision_diagnostics.bond_count;
    }
    return result;
}

/* Every firmware-initiated disconnect records why (string literal only).
 * The current link is asked to disconnect once; the request is repeated only
 * after 1 s if the DISCONNECTED event has not arrived (it resets on new epoch). */
static bool drop_requested;
static TickType_t drop_tick;
static void Drop(uint8_t handle, const char *reason)
{
    if (drop_requested && (TickType_t)(xTaskGetTickCount() - drop_tick) < pdMS_TO_TICKS(1000U))
    { return; }
    drop_requested = true;
    drop_tick = xTaskGetTickCount();
    app_provision_diagnostics.disconnect_reason = reason;
    ++app_provision_diagnostics.firmware_disconnects;
    (void)W6X_Ble_Disconnect(handle);
}

/* Bond table lives in the NCP (max W6X_BLE_MAX_BONDED_DEVICES entries). */
static void RefreshBondCount(void)
{
    static W6X_Ble_Bonded_Devices_Result_t bonds PROV_STORAGE;
    memset(&bonds, 0, sizeof(bonds));
    W6X_Status_t result = W6X_Ble_SecurityGetBondedDeviceList(&bonds);
    app_provision_diagnostics.bond_count = result == W6X_STATUS_OK ? bonds.Count : UINT32_MAX;
    memset(&bonds, 0, sizeof(bonds)); /* contains long-term keys */
}

void APP_ProvisionPoll(void)
{
    if (!initialized) { return; }
    uint32_t epoch;
    uint8_t handle, reject;
    bool connected, paired, subscribed, confirm, failed, poisoned, wifi_update;
    bool requested, enabled;
    taskENTER_CRITICAL();
    epoch = link.epoch; handle = link.handle; reject = link.reject_handle;
    connected = link.connected; paired = link.paired; subscribed = link.subscribed;
    confirm = link.confirm; failed = link.failed; poisoned = link.poisoned;
    wifi_update = wifi_changed; wifi_changed = false;
    requested = enable_request_pending; enabled = enable_requested;
    enable_request_pending = false;
    link.confirm = false; link.reject_handle = UINT8_MAX;
    taskEXIT_CRITICAL();
    bool was_open = window_open;
    (void)Window();
    if (requested)
    {
        if (enabled && !window_open)
        {
            stop_advertising_pending = false;
            boot_tick = xTaskGetTickCount();
            window_open = true;
            restart_advertising = !connected;
            advertising_tick = boot_tick - pdMS_TO_TICKS(1000U);
            if (connected) PublishEnabled(true);
        }
        else if (!enabled) { window_open = false; }
    }
    app_provision_diagnostics.window_open = window_open;
    app_provision_diagnostics.paired = paired;
    app_provision_diagnostics.subscribed = subscribed;
    if (reject != UINT8_MAX)
    {
        /* Different handle from the admitted link: not rate-limited by Drop. */
        app_provision_diagnostics.disconnect_reason = "second_client";
        ++app_provision_diagnostics.firmware_disconnects;
        (void)W6X_Ble_Disconnect(reject);
    }
    const bool link_changed = epoch != worker_epoch;
    if (link_changed)
    {
        worker_epoch = epoch;
        drop_requested = false;
        last_id = 0U;
        security_started = announced_paired = announced_subscribed = false;
        Prov_FrameReset(&frame);
        restart_advertising = !connected;
        RefreshBondCount();
        advertising_tick = xTaskGetTickCount() - pdMS_TO_TICKS(1000U);
    }
    if (!window_open)
    {
        PublishEnabled(false);
        restart_advertising = false;
        if (was_open || (requested && !enabled) || link_changed)
        {
            stop_advertising_pending = true;
            stop_advertising_tick = xTaskGetTickCount() - pdMS_TO_TICKS(1000U);
        }
        if (stop_advertising_pending &&
            (TickType_t)(xTaskGetTickCount() - stop_advertising_tick) >= pdMS_TO_TICKS(1000U))
        {
            stop_advertising_tick = xTaskGetTickCount();
            stop_advertising_pending = W6X_Ble_AdvStop() != W6X_STATUS_OK;
        }
        if (connected)
        {
            /* A fast OFF/ON must not reuse queued commands from the old link
               while the asynchronous disconnect event is still pending. */
            taskENTER_CRITICAL();
            if (link.epoch == epoch) { link.poisoned = true; }
            taskEXIT_CRITICAL();
            Drop(handle, "window_closed");
        }
        return;
    }
    if (!connected)
    {
        if (restart_advertising && window_open &&
            (TickType_t)(xTaskGetTickCount() - advertising_tick) >= pdMS_TO_TICKS(1000U))
        {
            advertising_tick = xTaskGetTickCount();
            /* Observed on SDK 2.0.89: the NCP resumes advertising by itself after
             * a disconnect and rejects a redundant ADVSTART with ERROR. Stop first
             * (result ignored: it may legitimately fail if not advertising). */
            (void)W6X_Ble_AdvStop();
            W6X_Status_t result = W6X_Ble_AdvStart();
            if (result == W6X_STATUS_OK)
            {
                restart_advertising = false;
                PublishEnabled(true);
            }
        }
        return;
    }
    if (poisoned || failed) { Drop(handle, poisoned ? "session_poisoned" : "pairing_failed"); return; }
    if (!security_started)
    {
        security_started = true;
        pair_tick = xTaskGetTickCount();
        (void)W6X_Ble_AdvStop();
        if (!Window()) { Drop(handle, "window_closed"); return; }
        /* BLE security level 2: encrypted Just Works, without MITM protection. */
        W6X_Status_t result = W6X_Ble_SecurityStart(handle, 2U);
        app_provision_diagnostics.security_status = (uint32_t)result;
        if (result != W6X_STATUS_OK) { Drop(handle, "security_start_failed"); return; }
    }
    if (confirm)
    {
        if (!Window() || W6X_Ble_SecurityPairingConfirm(handle) != W6X_STATUS_OK)
        { Drop(handle, "pair_confirm_failed"); return; }
    }
    if (!paired && (TickType_t)(xTaskGetTickCount() - pair_tick) >= pdMS_TO_TICKS(PAIR_MS))
    { Drop(handle, "pair_timeout"); return; }
    if (paired && !announced_paired) { RefreshBondCount(); }
    if (subscribed && (!announced_subscribed || paired != announced_paired || wifi_update || was_open != window_open))
    { (void)Status(0U, false); }
    announced_subscribed = subscribed;
    announced_paired = paired;
    if (frame.length != 0U && (TickType_t)(xTaskGetTickCount() - frame_tick) >= pdMS_TO_TICKS(FRAME_MS))
    {
        Prov_FrameReset(&frame);
        /* Discard the remainder through LF so a suffix is never a new command. */
        frame.dropping = true;
        Error(0U, "frame_timeout");
    }
    for (unsigned budget = 0U; budget < RX_SLOTS; ++budget)
    {
        RxChunk chunk;
        bool available;
        taskENTER_CRITICAL();
        available = link.count != 0U && link.epoch == worker_epoch;
        if (available)
        {
            chunk = link.rx[link.tail];
            Prov_Clear(&link.rx[link.tail], sizeof(chunk));
            link.tail = (link.tail + 1U) % RX_SLOTS;
            --link.count;
        }
        taskEXIT_CRITICAL();
        if (!available) { break; }
        if (chunk.epoch != worker_epoch || !Session(worker_epoch, &handle, true))
        { Prov_Clear(&chunk, sizeof(chunk)); break; }
        for (unsigned i = 0U; i < chunk.length; ++i)
        {
            if (!Session(worker_epoch, &handle, true)) { break; }
            if (frame.length == 0U) { frame_paired = chunk.paired; }
            frame_paired = frame_paired && chunk.paired;
            frame_tick = xTaskGetTickCount();
            ProvFrameResult result = Prov_FrameByte(&frame, chunk.data[i]);
            if (result == PROV_FRAME_BAD) { Error(0U, "invalid_frame"); }
            else if (result == PROV_FRAME_READY) { Dispatch(frame_paired); }
        }
        Prov_Clear(&chunk, sizeof(chunk));
    }
}
