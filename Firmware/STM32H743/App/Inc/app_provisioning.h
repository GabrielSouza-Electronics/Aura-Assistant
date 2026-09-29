#ifndef APP_PROVISIONING_H
#define APP_PROVISIONING_H

#include <stdbool.h>
#include "w6x_api.h"

/* ST67 AT GATT commands use compact hexadecimal UUIDs. The driver forwards
 * these strings unchanged. Phone APIs use the equivalent hyphenated UUIDs. */
#define AURA_PROV_SERVICE_UUID "7e57a001b5a34f3c9a7d2c6e8f901234"
#define AURA_PROV_COMMAND_UUID "7e57a002b5a34f3c9a7d2c6e8f901234"
#define AURA_PROV_EVENT_UUID   "7e57a003b5a34f3c9a7d2c6e8f901234"

typedef struct {
    uint32_t gatt_status;
    uint32_t adv_param_status;
    uint32_t security_status;
    uint32_t commands;
    uint32_t rejected;
    const char *last_error;  /* protocol error code of the last rejection */
    uint32_t rx_overflows;
    uint32_t tx_failures;
    uint32_t connect_status;
    uint32_t last_request_id;
    const char *last_command;  /* last accepted command name */
    const char *scan_stage;    /* progress of the current/last scan command */
    const char *disconnect_reason; /* why the firmware last dropped the BLE link */
    uint32_t firmware_disconnects;
    uint32_t bond_count;           /* NCP bond table entries (UINT32_MAX = query failed) */
    uint32_t bond_count_at_boot;   /* same, read once after init, before any connection */
    uint32_t mtu;                  /* ATT MTU reported for the current link */
    uint32_t tx_chunk;             /* notification payload size last used */
    uint32_t last_security_event;  /* 134 failed, 135 completed, 136 confirm, 137 canceled */
    uint32_t last_security_handle; /* handle reported with it (255 = unresolved address) */
    bool paired;
    bool subscribed;
    bool window_open;
} APP_ProvisionDiagnostics;
extern volatile APP_ProvisionDiagnostics app_provision_diagnostics;

/* Init and Poll run only in APP_WiFiTask. Callbacks never send AT commands. */
W6X_Status_t APP_ProvisionInit(void);
void APP_ProvisionPoll(void);
void APP_ProvisionBleEvent(W6X_event_id_t event_id, void *args,
                           uint8_t *rx_data, size_t rx_capacity);
void APP_ProvisionScanResult(int32_t status, W6X_WiFi_Scan_Result_t *results);
void APP_ProvisionWiFiEvent(void);

#endif
