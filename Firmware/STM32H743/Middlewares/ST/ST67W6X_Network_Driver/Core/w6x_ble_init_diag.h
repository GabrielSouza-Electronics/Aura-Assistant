/* Aura: credential-free BLE startup diagnostics for SWD/Watch.
 * Preserve with local patches when updating X-CUBE-ST67W61. */
#ifndef W6X_BLE_INIT_DIAG_H
#define W6X_BLE_INIT_DIAG_H
#include <stdint.h>
typedef enum
{
  W6X_BLE_INIT_NOT_STARTED = 0,
  W6X_BLE_INIT_CONTEXT,
  W6X_BLE_INIT_CALLBACKS,
  W6X_BLE_INIT_GET_POWER_MODE,
  W6X_BLE_INIT_GET_CLOCK_SOURCE,
  W6X_BLE_INIT_DISABLE_POWER_SAVE,
  W6X_BLE_INIT_REGISTER_CALLBACK,
  W6X_BLE_INIT_SEND_BLEINIT,
  W6X_BLE_INIT_VERIFY_MODE,
  W6X_BLE_INIT_SET_NAME,
  W6X_BLE_INIT_READY
} W6X_Ble_InitStep_t;
typedef struct
{
  W6X_Ble_InitStep_t step;
  uint32_t status;
  uint32_t cleanup_status; /* UINT32_MAX = not attempted. */
  uint32_t requested_mode;
  uint32_t reported_mode;
  uint32_t power_mode;
  uint32_t clock_source;
  uint32_t step_started_tick;
  uint32_t step_elapsed_ticks;
} W6X_Ble_InitDiagnostics_t;
extern volatile W6X_Ble_InitDiagnostics_t w6x_ble_init_diagnostics;
#endif
