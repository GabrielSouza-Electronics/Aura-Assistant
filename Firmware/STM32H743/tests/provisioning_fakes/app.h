#include <stdint.h>
typedef struct {
    uint32_t wifi_scan_status, wifi_scan_callback_status, wifi_ap_count, ble_adv_status;
    int32_t wifi_best_rssi;
} APP_WiFiDiagnostics_t;
extern volatile APP_WiFiDiagnostics_t app_wifi_diagnostics;
