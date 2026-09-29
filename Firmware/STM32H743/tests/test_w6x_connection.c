/* Executes the production W6X connect/disconnect functions with AT replies that
 * arrive before the command returns, as they can on a fast access point. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define ST67_ARCH 1
#define W6X_ARCH_T01 1
#define W6X_STATUS_OK 0
#define W6X_STATUS_ERROR 1
#define W61_STATUS_OK 0
#define W6X_WIFI_EVENT_FLAG_CONNECT 1U
#define W6X_WIFI_EVENT_FLAG_REASON 2U
#define W6X_WIFI_EVENT_FLAG_GOT_IP 4U
#define W6X_WIFI_EVENT_FLAG_DISCONNECT 8U
#define W6X_WIFI_CONNECT_TIMEOUT_MS 10000U
#define W6X_WIFI_CONNECT_WPS_TIMEOUT_MS 120000U
#define W6X_WIFI_GOT_IP_TIMEOUT_MS 15000U
#define W6X_WIFI_DISCONNECT_TIMEOUT_MS 5000U
#define W61_WIFI_STATE_AP_RUNNING 1
#define W6X_WIFI_STATE_STA_DISCONNECTED 0
#define W6X_WIFI_STATE_STA_CONNECTED 1
#define W6X_WIFI_STATE_STA_GOT_IP 2
#define pdTRUE 1
#define pdFALSE 0
#define pdMS_TO_TICKS(x) (x)
#define WIFI_LOG_ERROR(...) ((void)0)
#define WIFI_LOG_WARN(...) ((void)0)
#define WIFI_LOG_DEBUG(...) ((void)0)
#define NULL_ASSERT(p, message) do { if (!(p)) return W6X_STATUS_ERROR; } while (0)
typedef int W6X_Status_t;
typedef unsigned EventBits_t;
typedef struct { unsigned WPS; } W6X_WiFi_Connect_Opts_t;
typedef W6X_WiFi_Connect_Opts_t W61_WiFi_Connect_Opts_t;
typedef struct { void (*APP_wifi_cb)(void); } W6X_App_Cb_t;
static struct {
    int Wifi_event, StaState;
    unsigned Expected_event_connect, Expected_event_gotip, Expected_event_disconnect;
} context, *p_wifi_ctx = &context;
static struct {
    struct { unsigned Supported, DHCP_STA_IsEnabled; } NetCtx;
    struct { unsigned ApState; } WifiCtx;
    struct { unsigned WiFi_DTIM_Factor, WiFi_DTIM_Interval; } LowPowerCfg;
    struct { struct { void (*link_ap_up_fn)(void); void (*link_ap_down_fn)(void); } Netif_cb; } Callbacks;
} object, *W6X_WiFi_drv_obj = &object;
static void cb(void) {}
static W6X_App_Cb_t callbacks = {cb};
static unsigned bits, disconnect_calls, last_restore;
static int mode;
static W6X_App_Cb_t *W6X_GetCbHandler(void) { return &callbacks; }
static int TranslateErrorStatus(int s) { return s; }
static int W61_WiFi_SetDTIM(void *obj,unsigned n) { (void)obj;(void)n;return 0; }
static int W6X_GetPowerMode(uint32_t *mode_out) { *mode_out=0;return 0; }
static int W6X_SetPowerMode(uint32_t mode_in) { (void)mode_in;return 0; }
static unsigned xEventGroupClearBits(int event,unsigned mask) { (void)event;bits&=~mask;return bits; }
static unsigned xEventGroupWaitBits(int event,unsigned mask,int clear,int all,unsigned timeout)
{ (void)event;(void)all;(void)timeout;unsigned result=bits&mask;if(clear)bits&=~mask;return result; }
static int W61_WiFi_Connect(void *obj,W61_WiFi_Connect_Opts_t *opts)
{
    (void)obj;(void)opts;
    assert(context.Expected_event_connect==1 && context.Expected_event_gotip==1);
    assert(bits==0); /* Old transaction bits have been cleared. */
    if(mode==0) {
        bits|=W6X_WIFI_EVENT_FLAG_CONNECT|W6X_WIFI_EVENT_FLAG_GOT_IP;
        context.Expected_event_gotip=0;
        context.StaState=W6X_WIFI_STATE_STA_GOT_IP;
    }
    return mode==2 ? 1:0;
}
static int W61_WiFi_Disconnect(void *obj,unsigned restore)
{
    (void)obj;++disconnect_calls;last_restore=restore;
    if(context.Expected_event_disconnect)bits|=W6X_WIFI_EVENT_FLAG_DISCONNECT;
    return 0;
}
#include "w6x_connection_under_test.inc"
int main(void)
{
    object.NetCtx.DHCP_STA_IsEnabled=1;
    W6X_WiFi_Connect_Opts_t opts={0};
    bits=W6X_WIFI_EVENT_FLAG_REASON|W6X_WIFI_EVENT_FLAG_GOT_IP;
    assert(W6X_WiFi_Connect(&opts)==0);
    assert(!context.Expected_event_connect && !context.Expected_event_gotip);
    mode=1;bits=W6X_WIFI_EVENT_FLAG_CONNECT|W6X_WIFI_EVENT_FLAG_GOT_IP;
    assert(W6X_WiFi_Connect(&opts)!=0); /* Old success cannot satisfy a new attempt. */
    assert(!context.Expected_event_connect && !context.Expected_event_gotip);
    mode=2;assert(W6X_WiFi_Connect(&opts)!=0);
    assert(!context.Expected_event_connect && !context.Expected_event_gotip);
    context.StaState=W6X_WIFI_STATE_STA_DISCONNECTED;
    assert(W6X_WiFi_Disconnect(1)==0 && disconnect_calls==1 && last_restore==1);
    context.StaState=W6X_WIFI_STATE_STA_GOT_IP;
    assert(W6X_WiFi_Disconnect(0)==0 && disconnect_calls==2 && !context.Expected_event_disconnect);
    assert(W6X_WiFi_Connect(NULL)!=0);
    puts("W6X: early AT events, stale events, error cleanup and offline forget passed");
    return 0;
}
