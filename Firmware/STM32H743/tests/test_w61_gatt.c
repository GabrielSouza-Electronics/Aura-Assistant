#include "app_provisioning.h"
#include "w61_adv_diag.h"
volatile W61_AdvDiagnostics w61_adv_diagnostics;
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#define W61_CMDRSP_STRING_SIZE 192U
#define W61_BLE_TIMEOUT 2000U
#define W61_STATUS_ERROR 1
#define W61_NULL_ASSERT(p) do { if (!(p)) return W61_STATUS_ERROR; } while (0)
typedef int W61_Status_t;
typedef int W61_Object_t;
static char command[192];
static int transport_status;
static int W61_AT_Common_SetExecute(W61_Object_t *obj, uint8_t *cmd, uint32_t timeout)
{
    assert(obj && timeout == W61_BLE_TIMEOUT);
    assert(strlen((char *)cmd) < sizeof(command));
    strcpy(command, (char *)cmd);
    return transport_status;
}
#include "w61_gatt_under_test.inc"
int main(void)
{
    W61_Object_t obj = 0;
    W61_AdvObserve("ERROR");
    assert(w61_adv_diagnostics.terminal == 0);
    w61_adv_diagnostics.active = 1;
    W61_AdvObserve("ERR CODE:0x1234ABcd");
    assert(w61_adv_diagnostics.error_code_valid == 1);
    assert(w61_adv_diagnostics.error_code == 0x1234abcdU);
    W61_AdvObserve("ERROR");
    assert(w61_adv_diagnostics.terminal == 2);
    W61_AdvObserve("OK");
    assert(w61_adv_diagnostics.terminal == 1);
    w61_adv_diagnostics.error_code_valid = 0;
    W61_AdvObserve("ERR CODE:0x123456789");
    W61_AdvObserve("ERR CODE:0x");
    W61_AdvObserve("ERR CODE:0");
    W61_AdvObserve("ERR CODE:0xNOTHEX");
    W61_AdvObserve("+BLE:GATTWRITE:secret");
    assert(w61_adv_diagnostics.error_code_valid == 0);
    assert(w61_adv_diagnostics.terminal == 1);
    w61_adv_diagnostics.active = 0;
    assert(W61_Ble_SetAdvParam(&obj, 160, 320, 0, 7) == 0);
    assert(strcmp(command, "AT+BLEADVPARAM=160,320,0,7\r\n") == 0);
    assert(W61_Ble_CreateService(&obj, 0, AURA_PROV_SERVICE_UUID, 2) == 0);
    assert(strcmp(command,
        "AT+BLEGATTSSRVCRE=0,\"7e57a001b5a34f3c9a7d2c6e8f901234\",1,2\r\n") == 0);
    assert(W61_Ble_CreateCharacteristic(&obj, 0, 0, AURA_PROV_COMMAND_UUID, 2, 8, 2) == 0);
    assert(strcmp(command,
        "AT+BLEGATTSCHARCRE=0,0,\"7e57a002b5a34f3c9a7d2c6e8f901234\",8,2,2\r\n") == 0);
    assert(W61_Ble_CreateCharacteristic(&obj, 0, 1, AURA_PROV_EVENT_UUID, 2, 16, 1) == 0);
    assert(strcmp(command,
        "AT+BLEGATTSCHARCRE=0,1,\"7e57a003b5a34f3c9a7d2c6e8f901234\",16,1,2\r\n") == 0);
    transport_status = W61_STATUS_ERROR;
    assert(W61_Ble_CreateService(&obj, 0, AURA_PROV_SERVICE_UUID, 2) == W61_STATUS_ERROR);
    puts("W61 GATT: exact service/characteristic AT commands and error propagation passed");
    return 0;
}
