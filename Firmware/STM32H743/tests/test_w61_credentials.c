#include "w61_at_credential.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#define W61_STATUS_OK 0
#define W61_STATUS_ERROR 1
#define W61_WIFI_MAX_SSID_SIZE 32U
#define W61_WIFI_MAX_PASSWORD_SIZE 63U
#define W61_WIFI_RECONNECTION_INTERVAL 7200U
#define W61_WIFI_RECONNECTION_ATTEMPTS 1000U
#define W61_CMDRSP_STRING_SIZE 192U
#define W61_WIFI_CONNECT_TIMEOUT 1000U
#define W61_NCP_TIMEOUT 1000U
#define W61_NULL_ASSERT(p) do { if(!(p)) return W61_STATUS_ERROR; } while(0)
#define WIFI_LOG_ERROR(...) ((void)0)
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC2STR(m) (m)[0],(m)[1],(m)[2],(m)[3],(m)[4],(m)[5]
typedef int W61_Status_t;
typedef int W61_Object_t;
typedef struct {
    uint8_t SSID[33],Password[64],MAC[6];
    uint16_t Reconnection_interval,Reconnection_nb_attempts;
    uint32_t WPS,WEP,Secured;
} W61_WiFi_Connect_Opts_t;
static char output[512];
static unsigned calls;
static int W61_AT_Common_SetExecute(W61_Object_t *obj,uint8_t *cmd,uint32_t timeout)
{ (void)obj;(void)timeout;assert(strlen((char*)cmd)<256);strcpy(output,(char*)cmd);++calls;return 0; }
static int W61_WiFi_SetReconnectionOpts(W61_Object_t *obj,W61_WiFi_Connect_Opts_t *opts)
{ (void)obj;(void)opts;return 0; }
#include "w61_credentials_under_test.inc"
int main(void)
{
    W61_Object_t obj=0;
    W61_WiFi_Connect_Opts_t opts={0};
    strcpy((char*)opts.SSID,"a,\"\\");strcpy((char*)opts.Password,"12345678");
    assert(W61_WiFi_Connect(&obj,&opts)==0);
    assert(!strcmp(output,"AT+CWJAP=\"a\\,\\\"\\\\\",\"12345678\",,0\r\n"));
    memset(opts.SSID,'\\',32);opts.SSID[32]=0;
    memset(opts.Password,'"',63);opts.Password[63]=0;
    opts.MAC[0]=1;
    assert(W61_WiFi_Connect(&obj,&opts)==0);
    assert(strlen(output)>192 && strlen(output)<256);
    assert(strstr(output,"\"01:00:00:00:00:00\",0\r\n"));
    unsigned before=calls;
    strcpy((char*)opts.Password,"pw\r\nAT+RST");
    assert(W61_WiFi_Connect(&obj,&opts)!=0 && calls==before);
    memset(opts.Password,'p',64);
    assert(W61_WiFi_Connect(&obj,&opts)!=0 && calls==before);
    strcpy((char*)opts.SSID,"a\"b");
    assert(W61_WiFi_DeleteCredentials(&obj,opts.SSID)==0);
    assert(!strcmp(output,"AT+CWCREDDEL=\"a\\\"b\"\r\n"));
    puts("W61: exact AT escaping, maximum expanded credentials and injection rejection passed");
    return 0;
}
