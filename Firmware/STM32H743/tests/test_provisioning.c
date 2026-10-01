#include "app_provisioning.h"
#include "provision_protocol.h"
#include "FreeRTOS.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int critical_depth;
static uint32_t ticks;
static bool adv_params_set;
static unsigned adv_starts, adv_stops;
static bool fail_adv, fail_adv_stop;
static char output[16384];
static unsigned output_length, connects, disconnects, scans, deletes, restored;
static bool fail_send, fail_connect, silent_scan, drop_during_send, silent_disconnect;
static unsigned max_notify = 20, largest_notify;
static void (*scan_callback)(int32_t,W6X_WiFi_Scan_Result_t*);
static W6X_WiFi_StaStateType_e wifi_state = W6X_WIFI_STATE_STA_DISCONNECTED;
static W6X_WiFi_Ap_t access_point = {.SSID="Caf\xc3\xa9", .Security=W6X_WIFI_SECURITY_WPA2_PSK, .RSSI=-42, .Channel=6};
static void event(int id)
{
    W6X_Ble_CbParamData_t p = {0};
    p.charac_idx = 1;
    APP_ProvisionBleEvent(id, &p, NULL, 0);
    assert(critical_depth == 0);
}
static void scan_done(void)
{
    W6X_WiFi_Scan_Result_t results = {.Count=1,.AP=&access_point};
    scan_callback(0, &results);
}
TickType_t xTaskGetTickCount(void) { return ticks; }
void vTaskDelay(TickType_t n) { assert(!critical_depth); ticks += n; }
W6X_Status_t W6X_Ble_ServerNotify(uint8_t h,uint8_t s,uint8_t c,uint8_t *p,uint32_t n,uint32_t *sent,uint32_t timeout)
{
    (void)timeout; assert(!critical_depth && h==0 && s==0 && c==1 && n<=max_notify); if(n>largest_notify) largest_notify=n;
    if (fail_send) { *sent=0; return 1; }
    assert(output_length+n < sizeof(output));
    memcpy(output+output_length,p,n); output_length+=n; output[output_length]=0; *sent=n;
    if (drop_during_send) { drop_during_send=false; event(W6X_BLE_EVT_DISCONNECTED_ID); event(W6X_BLE_EVT_CONNECTED_ID); }
    return 0;
}
static void check_at_uuid(const char *uuid)
{
    assert(strlen(uuid) == 32);
    assert(strspn(uuid, "0123456789abcdefABCDEF") == 32);
}
W6X_Status_t W6X_Ble_CreateService(uint8_t i,const char *uuid,uint8_t type)
{ check_at_uuid(uuid); assert(!critical_depth && i==0 && type==2 && !strcmp(uuid,AURA_PROV_SERVICE_UUID)); return 0; }
W6X_Status_t W6X_Ble_CreateCharacteristic(uint8_t s,uint8_t c,const char *uuid,uint8_t type,uint8_t prop,uint8_t perm)
{ check_at_uuid(uuid); assert(!critical_depth && s==0 && type==2); assert(!strcmp(uuid,c==0?AURA_PROV_COMMAND_UUID:AURA_PROV_EVENT_UUID)); assert(prop==(c==0?8:16) && perm==(c==0?2:1)); return 0; }
W6X_Status_t W6X_Ble_RegisterCharacteristics(void) { assert(!critical_depth); return 0; }
W6X_Status_t W6X_Ble_SetSecurityParam(W6X_Ble_SecurityParameter_e p) { assert(p==3 && !critical_depth); return 0; }
W6X_Status_t W6X_Ble_SetAdvParam(uint32_t low, uint32_t high, W6X_Ble_AdvType_e type, W6X_Ble_AdvChannel_e channel)
{ assert(!critical_depth && low==160 && high==320 && type==0 && channel==7); adv_params_set=true; return 0; }
W6X_Status_t W6X_Ble_SetAdvData(const char *s) { assert(adv_params_set && !strcmp(s,"11073412908f6e2c7d9a3c4fa3b501a0577e050841757261") && strlen(s)/2U<=31U && !critical_depth); return 0; }
W6X_Status_t W6X_Ble_SetScanRespData(const char *s) { (void)s; assert(0 && "SDK 2.0.89 rejects AdvStart after a custom scan response"); return 0; }
W6X_Status_t W6X_Ble_Disconnect(uint32_t h) { assert(h==0 && !critical_depth); ++disconnects; if(!silent_disconnect) event(W6X_BLE_EVT_DISCONNECTED_ID); return 0; }
W6X_Status_t W6X_Ble_AdvStop(void) { assert(!critical_depth); ++adv_stops; return fail_adv_stop ? 1 : 0; }
W6X_Status_t W6X_Ble_AdvStart(void) { assert(!critical_depth); ++adv_starts; return fail_adv ? 1 : 0; }
W6X_Status_t W6X_Ble_SecurityStart(uint8_t h,uint8_t level) { assert(h==0 && level==2 && !critical_depth); return 0; }
static unsigned bond_queries;
W6X_Status_t W6X_Ble_SecurityGetBondedDeviceList(W6X_Ble_Bonded_Devices_Result_t *r)
{ assert(!critical_depth); ++bond_queries; r->Count=1; return 0; }
W6X_Status_t W6X_Ble_SecurityPairingConfirm(uint8_t h) { assert(h==0 && !critical_depth); return 0; }
W6X_Status_t W6X_WiFi_Station_GetState(W6X_WiFi_StaStateType_e *s,W6X_WiFi_Connect_t *info)
{ assert(!critical_depth); *s=wifi_state; if(info && wifi_state==W6X_WIFI_STATE_STA_GOT_IP) { strcpy((char*)info->SSID,"Caf\xc3\xa9"); } return 0; }
W6X_Status_t W6X_Net_Station_GetIPAddress(uint8_t *ip,uint8_t *gw,uint8_t *mask)
{ assert(!critical_depth); (void)gw;(void)mask; ip[0]=192;ip[1]=168;ip[2]=1;ip[3]=55; return 0; }
W6X_Status_t W6X_WiFi_Scan(W6X_WiFi_Scan_Opts_t *opts,void (*cb)(int32_t,W6X_WiFi_Scan_Result_t*))
{ assert(!critical_depth && opts->MaxCnt==10); ++scans; scan_callback=cb; if(!silent_scan) scan_done(); return 0; }
W6X_Status_t W6X_WiFi_Connect(W6X_WiFi_Connect_Opts_t *opts)
{ assert(!critical_depth && !strcmp((char*)opts->Password,"12345678")); ++connects; wifi_state=fail_connect?W6X_WIFI_STATE_STA_DISCONNECTED:W6X_WIFI_STATE_STA_GOT_IP; return fail_connect?1:0; }
W6X_Status_t W6X_WiFi_Disconnect(uint32_t restore) { assert(!critical_depth); restored+=restore; wifi_state=W6X_WIFI_STATE_STA_DISCONNECTED; return 0; }
W6X_Status_t W6X_WiFi_GetCredentials(W6X_WiFi_CredentialsList_t *list)
{ assert(!critical_depth); list->Count=deletes?0:1; strcpy((char*)list->SSID[0],"Caf\xc3\xa9"); return 0; }
W6X_Status_t W6X_WiFi_DeleteCredentials(uint8_t *ssid) { assert(!critical_depth && ssid[0]); ++deletes; return 0; }
static void clear_output(void) { output_length=0;output[0]=0; }
static void chunk(const char *data,size_t n,bool poll)
{
    uint8_t buffer[256]={0}; assert(n<=sizeof(buffer)); memcpy(buffer,data,n);
    W6X_Ble_CbParamData_t p={0}; p.available_data_length=(uint32_t)n;
    APP_ProvisionBleEvent(W6X_BLE_EVT_WRITE_ID,&p,buffer,sizeof(buffer));
    for(size_t i=0;i<sizeof(buffer);++i) assert(buffer[i]==0);
    if(poll) APP_ProvisionPoll();
}
static void request(const char *data)
{
    clear_output();
    for(size_t offset=0;offset<strlen(data);)
    { size_t n=strlen(data)-offset; if(n>20)n=20; chunk(data+offset,n,true); offset+=n; }
}
static void start(bool paired)
{
    assert(APP_ProvisionInit()==0);
    assert(!APP_ProvisionIsEnabled());
    APP_ProvisionRequestEnabled(true); APP_ProvisionPoll();
    assert(APP_ProvisionIsEnabled());
    event(W6X_BLE_EVT_CONNECTED_ID); APP_ProvisionPoll();
    event(W6X_BLE_EVT_NOTIFICATION_STATUS_ENABLED_ID);
    if(paired) event(W6X_BLE_EVT_PAIRING_COMPLETED_ID);
    APP_ProvisionPoll(); clear_output();
}
int main(int argc,char **argv)
{
    assert(argc==2);
    if(!strcmp(argv[1],"happy"))
    {
        start(true);
        request("{\"v\":1,\"id\":1,\"cmd\":\"scan\"}\n");
        assert(scans==1 && strstr(output,"scan_started") && strstr(output,"Caf\xc3\xa9") && strstr(output,"scan_done"));
        assert(!strcmp(app_provision_diagnostics.last_command,"scan") && !strcmp(app_provision_diagnostics.scan_stage,"done"));
        request("{\"v\":1,\"id\":2,\"cmd\":\"connect\",\"ssid\":\"Caf\\u00e9\",\"password\":\"12345678\"}\n");
        assert(connects==1 && strstr(output,"\"event\":\"connected\"") && strstr(output,"192.168.1.55"));
        request("{\"v\":1,\"id\":2,\"cmd\":\"connect\",\"ssid\":\"Caf\\u00e9\",\"password\":\"12345678\"}\n");
        assert(connects==1 && strstr(output,"stale_id"));
        request("{\"v\":1,\"id\":3,\"cmd\":\"forget\"}\n");
        assert(restored==1 && deletes==1 && strstr(output,"forgotten"));
    }
    else if(!strcmp(argv[1],"security"))
    {
        start(false);
        request("{\"v\":1,\"id\":1,\"cmd\":\"connect\",\"ssid\":\"x\",\"password\":\"12345678\"}\n");
        assert(!connects && strstr(output,"pairing_required"));
        event(W6X_BLE_EVT_PAIRING_FAILED_ID); APP_ProvisionPoll(); assert(disconnects==1);
        assert(!strcmp(app_provision_diagnostics.disconnect_reason,"pairing_failed") && app_provision_diagnostics.last_security_event==134);
    }
    else if(!strcmp(argv[1],"timeout"))
    {
        start(true); chunk("{\"v\":",5,true); ticks+=10001; APP_ProvisionPoll();
        assert(strstr(output,"frame_timeout")); chunk("1}\n",3,true);
        request("{\"v\":1,\"id\":1,\"cmd\":\"get_status\"}\n"); assert(strstr(output,"\"event\":\"status\""));
        ticks=300001;
        APP_ProvisionPoll();
        assert(!scans && disconnects==1 && !APP_ProvisionIsEnabled());
    }
    else if(!strcmp(argv[1],"settings"))
    {
        assert(APP_ProvisionInit()==0); APP_ProvisionPoll();
        assert(!APP_ProvisionIsEnabled() && adv_starts==0);
        fail_adv=true;
        APP_ProvisionRequestEnabled(true); APP_ProvisionPoll();
        assert(!APP_ProvisionIsEnabled() && adv_starts==1);
        fail_adv=false; ticks+=1000; APP_ProvisionPoll();
        assert(APP_ProvisionIsEnabled() && adv_starts==2);
        ticks=299999; APP_ProvisionRequestEnabled(true); APP_ProvisionPoll();
        assert(APP_ProvisionIsEnabled()); // Repeated UP does not extend window.
        ticks=300000; APP_ProvisionPoll();
        assert(!APP_ProvisionIsEnabled() && !app_provision_diagnostics.window_open);
        APP_ProvisionRequestEnabled(true); APP_ProvisionPoll();
        assert(APP_ProvisionIsEnabled()); // Reopen without reboot.
        event(W6X_BLE_EVT_CONNECTED_ID); APP_ProvisionPoll();
        APP_ProvisionRequestEnabled(false); APP_ProvisionPoll();
        assert(!APP_ProvisionIsEnabled() && disconnects==1);
        APP_ProvisionPoll();
        unsigned starts=adv_starts;
        ticks+=2000; APP_ProvisionPoll(); assert(adv_starts==starts);
        assert(adv_stops>0);
        APP_ProvisionRequestEnabled(true); APP_ProvisionPoll();
        fail_adv_stop=true;
        APP_ProvisionRequestEnabled(false); APP_ProvisionPoll();
        unsigned stops=adv_stops;
        APP_ProvisionPoll(); assert(adv_stops==stops);
        fail_adv_stop=false; ticks+=1000; APP_ProvisionPoll();
        assert(adv_stops==stops+1 && !APP_ProvisionIsEnabled());
    }
    else if(!strcmp(argv[1],"overflow"))
    {
        start(true);
        for(unsigned i=0;i<49;++i) chunk("x",1,false);
        APP_ProvisionPoll(); assert(disconnects==1 && app_provision_diagnostics.rx_overflows==1);
    }
    else if(!strcmp(argv[1],"oversize"))
    { start(true); chunk("123456789012345678901",21,true); assert(disconnects==1); }
    else if(!strcmp(argv[1],"scan_timeout"))
    {
        start(true); silent_scan=true;
        request("{\"v\":1,\"id\":1,\"cmd\":\"scan\"}\n"); assert(strstr(output,"scan_timeout"));
        request("{\"v\":1,\"id\":2,\"cmd\":\"scan\"}\n"); assert(scans==1 && strstr(output,"scan_busy"));
        scan_done(); silent_scan=false;
        request("{\"v\":1,\"id\":3,\"cmd\":\"scan\"}\n"); assert(scans==2 && strstr(output,"scan_done"));
    }
    else if(!strcmp(argv[1],"send_failure"))
    { start(true); fail_send=true; request("{\"v\":1,\"id\":1,\"cmd\":\"scan\"}\n"); APP_ProvisionPoll(); assert(!scans && disconnects==1); }
    else if(!strcmp(argv[1],"stale_session"))
    {
        start(true); drop_during_send=true;
        request("{\"v\":1,\"id\":1,\"cmd\":\"scan\"}\n"); assert(!scans && output_length==20);
        APP_ProvisionPoll(); event(W6X_BLE_EVT_NOTIFICATION_STATUS_ENABLED_ID); event(W6X_BLE_EVT_PAIRING_COMPLETED_ID); APP_ProvisionPoll();
        request("{\"v\":1,\"id\":1,\"cmd\":\"get_status\"}\n"); assert(strstr(output,"\"event\":\"status\""));
    }
    else if(!strcmp(argv[1],"wifi_failure"))
    {
        start(true); fail_connect=true;
        request("{\"v\":1,\"id\":1,\"cmd\":\"connect\",\"ssid\":\"x\",\"password\":\"12345678\"}\n");
        assert(connects==1 && strstr(output,"connection_failed") && !strstr(output,"\"event\":\"connected\""));
    }
    else if(!strcmp(argv[1],"rpa_pairing"))
    {
        /* Driver could not map the identity address to a handle: conn_handle 0xFF. */
        start(false);
        W6X_Ble_CbParamData_t p = {0};
        p.remote_ble_device.conn_handle = 0xFF;
        APP_ProvisionBleEvent(W6X_BLE_EVT_PAIRING_COMPLETED_ID, &p, NULL, 0); APP_ProvisionPoll();
        assert(app_provision_diagnostics.paired);
        assert(app_provision_diagnostics.bond_count==1 && app_provision_diagnostics.last_security_handle==0xFF);
        request("{\"v\":1,\"id\":1,\"cmd\":\"scan\"}\n");
        assert(scans==1 && strstr(output,"scan_done") && !strstr(output,"pairing_required"));
        /* An unresolved non-security event must still be ignored. */
        p.available_data_length = 1;
        APP_ProvisionBleEvent(W6X_BLE_EVT_WRITE_ID, &p, (uint8_t[256]){'x'}, 256);
        APP_ProvisionPoll(); assert(disconnects==0);
    }
    else if(!strcmp(argv[1],"mtu"))
    {
        /* Notifications follow the negotiated MTU (capped at 244); writes stay at 20. */
        start(true);
        W6X_Ble_CbParamData_t p = {0}; p.mtu_size = 517; max_notify = 244;
        APP_ProvisionBleEvent(W6X_BLE_EVT_MTU_SIZE_ID, &p, NULL, 0);
        request("{\"v\":1,\"id\":1,\"cmd\":\"scan\"}\n");
        assert(scans==1 && strstr(output,"scan_done") && largest_notify>20 && largest_notify<=244);
        assert(app_provision_diagnostics.mtu==517 && app_provision_diagnostics.tx_chunk==244);
    }
    else if(!strcmp(argv[1],"drop_once"))
    {
        /* Without a DISCONNECTED event the request is repeated only after 1 s. */
        start(false); silent_disconnect = true;
        event(W6X_BLE_EVT_PAIRING_FAILED_ID);
        for(int i=0;i<5;++i) { APP_ProvisionPoll(); ticks+=10; }
        assert(disconnects==1);
        ticks+=1000; APP_ProvisionPoll(); assert(disconnects==2);
        assert(app_provision_diagnostics.firmware_disconnects==2);
    }
    else { assert(!"unknown scenario"); }
    assert(!critical_depth);
    printf("provisioning: %s passed\n",argv[1]);
    return 0;
}
