#ifndef PROV_FAKE_W6X_H
#define PROV_FAKE_W6X_H
#include <stdint.h>
#include <stddef.h>
typedef int W6X_Status_t;
typedef enum { W6X_BLE_ADV_TYPE_IND = 0 } W6X_Ble_AdvType_e;
typedef enum { W6X_BLE_ADV_CHANNEL_ALL = 7 } W6X_Ble_AdvChannel_e;
W6X_Status_t W6X_Ble_SetAdvParam(uint32_t, uint32_t, W6X_Ble_AdvType_e, W6X_Ble_AdvChannel_e);
typedef int W6X_event_id_t;
#define W6X_STATUS_OK 0
#define W6X_STATUS_ERROR 1
#define W6X_WIFI_MAX_SSID_LIST_SIZE 5U
enum {
W6X_BLE_EVT_CONNECTED_ID=121, W6X_BLE_EVT_DISCONNECTED_ID=122,
W6X_BLE_EVT_WRITE_ID=125, W6X_BLE_EVT_NOTIFICATION_STATUS_ENABLED_ID=130,
W6X_BLE_EVT_NOTIFICATION_STATUS_DISABLED_ID=131, W6X_BLE_EVT_MTU_SIZE_ID=133,
W6X_BLE_EVT_PAIRING_FAILED_ID=134, W6X_BLE_EVT_PAIRING_COMPLETED_ID=135,
W6X_BLE_EVT_PAIRING_CONFIRM_ID=136, W6X_BLE_EVT_PAIRING_CANCELED_ID=137
};
typedef enum { W6X_WIFI_SECURITY_OPEN, W6X_WIFI_SECURITY_WEP, W6X_WIFI_SECURITY_WPA_PSK,
W6X_WIFI_SECURITY_WPA2_PSK, W6X_WIFI_SECURITY_WPA_WPA2_PSK, W6X_WIFI_SECURITY_WPA_ENT,
W6X_WIFI_SECURITY_WPA3_SAE, W6X_WIFI_SECURITY_WPA2_WPA3_SAE, W6X_WIFI_SECURITY_UNKNOWN } W6X_WiFi_SecurityType_e;
typedef enum { W6X_WIFI_STATE_STA_NO_STARTED_CONNECTION, W6X_WIFI_STATE_STA_CONNECTED,
W6X_WIFI_STATE_STA_GOT_IP, W6X_WIFI_STATE_STA_CONNECTING, W6X_WIFI_STATE_STA_DISCONNECTED } W6X_WiFi_StaStateType_e;
typedef int W6X_Ble_SecurityParameter_e;
enum { W6X_BLE_UUID_TYPE_128=2, W6X_BLE_CHAR_PROP_WRITE_WITH_RESP=8,
W6X_BLE_CHAR_PERM_WRITE=2, W6X_BLE_CHAR_PROP_NOTIFY=16, W6X_BLE_CHAR_PERM_READ=1,
W6X_BLE_SEC_IO_NO_INPUT_OUTPUT=3 };
typedef struct { uint8_t SSID[33]; W6X_WiFi_SecurityType_e Security; int16_t RSSI; uint8_t MAC[6], Channel; } W6X_WiFi_Ap_t;
typedef struct { uint32_t Count; W6X_WiFi_Ap_t *AP; } W6X_WiFi_Scan_Result_t;
typedef struct { uint8_t MaxCnt; } W6X_WiFi_Scan_Opts_t;
typedef struct { uint8_t SSID[33], Password[64]; uint16_t Reconnection_interval, Reconnection_nb_attempts; } W6X_WiFi_Connect_Opts_t;
typedef struct { uint8_t SSID[33]; } W6X_WiFi_Connect_t;
typedef struct { uint32_t Count; uint8_t SSID[5][33]; } W6X_WiFi_CredentialsList_t;
typedef struct { uint8_t conn_handle; } W6X_Ble_Device_t;
typedef struct { uint8_t service_idx, charac_idx; uint16_t mtu_size; uint32_t available_data_length; W6X_Ble_Device_t remote_ble_device; } W6X_Ble_CbParamData_t;
W6X_Status_t W6X_Ble_ServerNotify(uint8_t,uint8_t,uint8_t,uint8_t*,uint32_t,uint32_t*,uint32_t);
W6X_Status_t W6X_Ble_CreateService(uint8_t,const char*,uint8_t);
W6X_Status_t W6X_Ble_CreateCharacteristic(uint8_t,uint8_t,const char*,uint8_t,uint8_t,uint8_t);
W6X_Status_t W6X_Ble_RegisterCharacteristics(void);
W6X_Status_t W6X_Ble_SetSecurityParam(W6X_Ble_SecurityParameter_e);
W6X_Status_t W6X_Ble_SetAdvData(const char*);
W6X_Status_t W6X_Ble_SetScanRespData(const char*);
W6X_Status_t W6X_Ble_Disconnect(uint32_t);
W6X_Status_t W6X_Ble_AdvStop(void);
W6X_Status_t W6X_Ble_AdvStart(void);
W6X_Status_t W6X_Ble_SecurityStart(uint8_t,uint8_t);
W6X_Status_t W6X_Ble_SecurityPairingConfirm(uint8_t);
typedef struct { uint8_t BDAddr[6]; uint8_t bd_addr_type; uint8_t LongTermKey[32]; } W6X_Ble_Bonded_Device_t;
typedef struct { uint32_t Count; W6X_Ble_Bonded_Device_t Bonded_device[2]; } W6X_Ble_Bonded_Devices_Result_t;
W6X_Status_t W6X_Ble_SecurityGetBondedDeviceList(W6X_Ble_Bonded_Devices_Result_t*);
W6X_Status_t W6X_WiFi_Station_GetState(W6X_WiFi_StaStateType_e*,W6X_WiFi_Connect_t*);
W6X_Status_t W6X_Net_Station_GetIPAddress(uint8_t*,uint8_t*,uint8_t*);
W6X_Status_t W6X_WiFi_Scan(W6X_WiFi_Scan_Opts_t*,void (*)(int32_t,W6X_WiFi_Scan_Result_t*));
W6X_Status_t W6X_WiFi_Connect(W6X_WiFi_Connect_Opts_t*);
W6X_Status_t W6X_WiFi_Disconnect(uint32_t);
W6X_Status_t W6X_WiFi_GetCredentials(W6X_WiFi_CredentialsList_t*);
W6X_Status_t W6X_WiFi_DeleteCredentials(uint8_t*);
#endif
