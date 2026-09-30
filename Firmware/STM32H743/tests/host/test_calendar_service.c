/* Include unchanged production implementation to exercise its private worker
 * with deterministic sockets/time, without creating a real RTOS thread. */
#include "app_calendar.c"
#include <assert.h>
#include <setjmp.h>

static uint32_t tick=UINT32_MAX-1500U, elapsed, polls;
static unsigned time_requests,holiday_requests,closes,credentials;
static bool bad_holidays, receive_timeout, connect_failure;
static char tx[512],rx[1024];
static size_t tx_size,rx_size,rx_pos;
static void (*entrypoint)(void *);
static jmp_buf finished;

TickType_t xTaskGetTickCount(void) { return tick; }
osThreadId_t osThreadNew(void (*entry)(void *),void *arg,const osThreadAttr_t *attr)
{
    (void)arg; assert(attr->stack_size>=4096 && attr->cb_mem && attr->stack_mem);
    entrypoint=entry; return (void *)1;
}
int W6X_Net_SNTP_SetConfiguration(uint8_t enabled,int16_t zone,uint8_t *a,uint8_t *b,uint8_t *c)
{ assert(enabled==1 && zone==0 && a && !b && !c); return 0; }
int W6X_Net_ResolveHostAddress(const char *host,uint8_t *ip)
{ assert(host); memset(ip,1,4); return 0; }
int W6X_Net_Socket(int a,int b,int c)
{ assert(a==AF_INET && b==SOCK_STREAM && c==IPPROTO_TLS_1_2); tx_size=rx_size=rx_pos=0; tx[0]=0; return 3; }
int W6X_Net_TLS_Credential_AddByContent(uint32_t tag,int type,const char *name,const char *pem,uint32_t len)
{ (void)type; assert(tag==3 && name && strstr(pem,"BEGIN CERTIFICATE") && len==strlen(pem)); ++credentials; return 0; }
int W6X_Net_TLS_Credential_Delete(uint32_t tag,int type)
{ (void)type; assert(tag==3 && credentials); --credentials; return 0; }
int W6X_Net_Setsockopt(int sock,int level,int opt,const void *data,size_t n)
{ assert(sock==3 && data); if (level==SOL_TLS && opt==TLS_SEC_TAG_LIST) assert(n==1); return 0; }
int W6X_Net_Connect(int sock,const struct sockaddr *address,size_t n)
{ assert(sock==3 && address && n); return connect_failure?-1:0; }
int W6X_Net_Close(int sock) { assert(sock==3); ++closes; return 0; }
ssize_t W6X_Net_Send(int sock,const void *data,size_t n,int flags)
{
    (void)flags; assert(sock==3);
    if (n>7) n=7; // Exercise partial sends.
    memcpy(tx+tx_size,data,n); tx_size+=n; tx[tx_size]=0;
    if (strstr(tx,"\r\n\r\n")) {
        char body[600];
        assert(strstr(tx,"Accept-Encoding: identity"));
        if (strstr(tx,"GET /api/Time/")) {
            ++time_requests; assert(strstr(tx,"Asia%2FDubai"));
            CalDate now={2026,9,30,12,0,0}; Cal_Advance(&now,elapsed/1000);
            snprintf(body,sizeof(body),"{\"year\":%u,\"month\":%u,\"day\":%u,\"hour\":%u,\"minute\":%u,\"seconds\":%u,\"timeZone\":\"Asia/Dubai\"}",
                now.year,now.month,now.day,now.hour,now.minute,now.second);
            rx_size=snprintf(rx,sizeof(rx),"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n%x\r\n%s\r\n0\r\n\r\n",(unsigned)strlen(body),body);
        } else {
            ++holiday_requests; unsigned year=0; const char *p=strstr(tx,"year="); assert(p); sscanf(p+5,"%u",&year);
            snprintf(body,sizeof(body),"{\"country\":\"AE\",\"year\":%u,\"years\":[%u],\"count\":1,\"holidays\":[{\"type\":\"public\",\"date\":\"%u-12-02\",\"confidence\":\"generated\"}]}",year,year,year);
            if (bad_holidays) strcpy(body,"{truncated");
            rx_size=snprintf(rx,sizeof(rx),"HTTP/1.1 200 OK\r\nContent-Length: %u\r\n\r\n%s",(unsigned)strlen(body),body);
        }
    }
    return (ssize_t)n;
}
ssize_t W6X_Net_Recv(int sock,void *data,size_t n,int flags)
{
    (void)flags; assert(sock==3);
    if (receive_timeout) return 0;
    if (n>17) n=17;
    if (n>rx_size-rx_pos) n=rx_size-rx_pos;
    memcpy(data,rx+rx_pos,n); rx_pos+=n; return (ssize_t)n;
}
static void advance(uint32_t ms) { tick+=ms; elapsed+=ms; }
void osDelay(uint32_t ticks)
{
    advance(ticks); ++polls;
    CalSnapshot s; APP_CalendarRead(&s);
    assert(s.time_valid);
    switch (polls) {
    case 2:
        assert(time_requests==1 && holiday_requests==1 && s.holidays_valid);
        assert(s.now.second==2); // Includes TickType_t wraparound.
        APP_CalendarRequestYear(2027); break;
    case 3:
        assert(holiday_requests==2 && s.holidays.year==2027);
        APP_CalendarRequestYear(2026); break;
    case 4: assert(holiday_requests==2 && s.holidays.year==2026); break;
    case 5: advance(86400000); break;
    case 6:
        assert(time_requests==2 && holiday_requests==3 && s.now.day==1 && !s.stale);
        APP_CalendarSetOnline(false); advance(86400000); break;
    case 7:
        assert(time_requests==2 && holiday_requests==3 && s.stale && !s.online && s.now.day==2);
        APP_CalendarSetOnline(true); bad_holidays=true; break;
    case 8:
        assert(time_requests==3 && holiday_requests==4 && s.stale && s.holidays_valid);
        assert(s.holidays.days[11]&2U); bad_holidays=false; break;
    case 20: assert(holiday_requests==4); break; // Retry backoff, no busy loop.
    case 50:
        assert(time_requests==3 && holiday_requests==5 && !s.stale && s.holidays_valid);
        assert(closes==8 && credentials==0);
        longjmp(finished,1);
    }
}
int main(void)
{
    uint32_t saved_tick=tick;
    tick=0;
    CalSnapshot s; APP_CalendarRead(&s);
    assert(s.time_valid && s.stale && !s.holidays_valid);
    assert(s.now.year==2026 && s.now.month==9 && s.now.day==30);
    assert(s.now.hour==23 && s.now.minute==24 && s.now.second==0);
    tick=90000; APP_CalendarRead(&s);
    assert(s.now.hour==23 && s.now.minute==25 && s.now.second==30);
    tick=saved_tick;
    assert(APP_CalendarStart()); assert(APP_CalendarStart());
    APP_CalendarSetOnline(true);
    if (!setjmp(finished)) entrypoint(NULL);
    APP_CalendarRead(&s);
    CalDate expected=s.now;
    Cal_Advance(&expected,90); advance(90000);
    APP_CalendarRead(&s);
    assert(s.now.minute==expected.minute && s.now.second==expected.second);
    char *json=NULL;
    receive_timeout=true;
    assert(!get_json(false,0,&json) && credentials==0);
    receive_timeout=false; connect_failure=true;
    assert(!get_json(false,0,&json) && credentials==0);
    assert(closes==10);
    puts("Calendar service: daily refresh, cached years, offline clock, tick wrap, failed fetch preservation, retry and TLS/socket cleanup passed.");
}
