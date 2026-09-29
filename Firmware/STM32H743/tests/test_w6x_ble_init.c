#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "w6x_ble_init_diag.h"

typedef enum { W6X_STATUS_OK, W6X_STATUS_BUSY, W6X_STATUS_ERROR,
               W6X_STATUS_TIMEOUT } W6X_Status_t;
typedef unsigned W6X_Ble_Mode_e;
typedef unsigned W61_Ble_Mode_e;
typedef struct {
    struct { struct { unsigned Mode; } NetSettings; } BleCtx;
    struct { unsigned Ble_status; } ResetCfg;
} W61_Object_t;
typedef struct { void (*APP_ble_cb)(void); } W6X_App_Cb_t;
#define W6X_BLE_DEVICE_NAME_SIZE 32
#define W6X_BLE_HOSTNAME "AuraAssistant"
#define W61_MODULE_STATE_INIT 1
#define W61_MODULE_STATE_NOT_INIT 0
#define BLE_LOG_ERROR(...) ((void)0)
#define BLE_LOG_WARN(...) ((void)0)
#define TranslateErrorStatus(value) (value)
static W61_Object_t object;
static W61_Object_t *W6X_Ble_drv_obj;
static void callback(void) {}
static W6X_App_Cb_t callbacks = { callback };
static W6X_Ble_InitStep_t fail_step;
static unsigned reported_mode, name_calls, init_calls, cleanup_calls;
static uint32_t tick;
static uint32_t xTaskGetTickCount(void) { return tick++; }
static W61_Object_t *W61_ObjGet(void) { return &object; }
static W6X_App_Cb_t *W6X_GetCbHandler(void) { return &callbacks; }
static W6X_Status_t result(W6X_Ble_InitStep_t step) {
    return fail_step == step ? W6X_STATUS_TIMEOUT : W6X_STATUS_OK;
}
static W6X_Status_t W6X_GetPowerMode(uint32_t *mode) {
    *mode = 0; return result(W6X_BLE_INIT_GET_POWER_MODE);
}
static W6X_Status_t W61_GetClockSource(W61_Object_t *obj, uint32_t *clock) {
    (void)obj; *clock = 0; return W6X_STATUS_OK;
}
static W6X_Status_t W6X_SetPowerMode(unsigned mode) {
    (void)mode; return W6X_STATUS_OK;
}
static void W6X_Ble_cb(void) {}
static W6X_Status_t W61_RegisterULcb(W61_Object_t *obj, void *a, void *b,
                                    void *c, void *d, void (*cb)(void)) {
    (void)obj; (void)a; (void)b; (void)c; (void)d; (void)cb;
    return result(W6X_BLE_INIT_REGISTER_CALLBACK);
}
static W6X_Status_t W61_Ble_Init(W61_Object_t *obj, uint8_t mode,
                                uint8_t *data, uint32_t len) {
    (void)obj; (void)mode; (void)data; (void)len; ++init_calls;
    return result(W6X_BLE_INIT_SEND_BLEINIT);
}
static W6X_Status_t W6X_Ble_GetInitMode(W6X_Ble_Mode_e *mode) {
    *mode = reported_mode; return result(W6X_BLE_INIT_VERIFY_MODE);
}
static W6X_Status_t W6X_Ble_SetDeviceName(char *name) {
    assert(strcmp(name, W6X_BLE_HOSTNAME) == 0); ++name_calls;
    return result(W6X_BLE_INIT_SET_NAME);
}
static W6X_Status_t W61_Ble_DeInit(W61_Object_t *obj) {
    (void)obj; ++cleanup_calls; return W6X_STATUS_ERROR;
}
void W6X_Ble_DeInit(void);
#include "w6x_ble_init_under_test.inc"

int main(void) {
    const W6X_Ble_InitStep_t failures[] = {
        W6X_BLE_INIT_GET_POWER_MODE, W6X_BLE_INIT_REGISTER_CALLBACK,
        W6X_BLE_INIT_SEND_BLEINIT, W6X_BLE_INIT_VERIFY_MODE,
        W6X_BLE_INIT_SET_NAME
    };
    reported_mode = 2;
    assert(W6X_Ble_Init(2, NULL, 0) == W6X_STATUS_OK);
    assert(w6x_ble_init_diagnostics.step == W6X_BLE_INIT_READY);
    for (size_t i = 0; i < sizeof(failures) / sizeof(failures[0]); ++i) {
        fail_step = failures[i]; name_calls = init_calls = cleanup_calls = 0;
        assert(W6X_Ble_Init(2, NULL, 0) == W6X_STATUS_TIMEOUT);
        assert(w6x_ble_init_diagnostics.step == fail_step);
        assert(w6x_ble_init_diagnostics.status == W6X_STATUS_TIMEOUT);
        if (fail_step < W6X_BLE_INIT_SEND_BLEINIT) {
            assert(init_calls == 0 && cleanup_calls == 0);
            assert(w6x_ble_init_diagnostics.cleanup_status == UINT32_MAX);
        } else {
            assert(cleanup_calls == 1);
            assert(w6x_ble_init_diagnostics.cleanup_status == W6X_STATUS_ERROR);
        }
        if (fail_step < W6X_BLE_INIT_SET_NAME) { assert(name_calls == 0); }
    }
    fail_step = W6X_BLE_INIT_NOT_STARTED; reported_mode = 1; name_calls = 0;
    assert(W6X_Ble_Init(2, NULL, 0) == W6X_STATUS_ERROR);
    assert(w6x_ble_init_diagnostics.step == W6X_BLE_INIT_VERIFY_MODE);
    assert(name_calls == 0);
    callbacks.APP_ble_cb = NULL;
    assert(W6X_Ble_Init(2, NULL, 0) == W6X_STATUS_ERROR);
    assert(w6x_ble_init_diagnostics.step == W6X_BLE_INIT_CALLBACKS);
    puts("BLE startup diagnostics and error propagation: PASS");
    return 0;
}
