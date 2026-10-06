#include "app_calendar.h"
#include "calendar_certificates.h"
#include "app_tasks.h"
#include "app_reminders.h"
#include "app_web_config.h"
#include "web_state.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"
#include "w6x_api.h"
#include <stdio.h>
#include <string.h>

/* Explicitly initialized NOLOAD storage in existing SRAM4. No DMA uses it.
 * Avoid consuming the nearly full DTCM with HTTP buffers/task stack. */
#define CAL_RAM __attribute__((section(".calendar"), aligned(8)))
#define RESPONSE_CAP 32768U
typedef struct { CalHolidays holidays; uint32_t day; } Cache;
static struct {
    char response[RESPONSE_CAP+1];
    Cache cache[3];
    CalSnapshot shared;
    uint16_t requested_year;
    bool online;
    uint32_t clock_tick, sync_day, shared_tick;
    WebState web;
} state CAL_RAM;
static StackType_t calendar_stack[1536] CAL_RAM;
static StaticTask_t calendar_task CAL_RAM;
static osThreadId_t worker;

/* User-supplied boot fallback, local Abu Dhabi time (UTC+4).
 * This is a seed, not a persistent RTC; HTTP replaces it on first connection. */
static void boot_snapshot(CalSnapshot *out)
{
    memset(out,0,sizeof(*out));
    const CalDate initial={2026,9,30,23,24,0};
    out->now=initial;
    Cal_Advance(&out->now,xTaskGetTickCount()/configTICK_RATE_HZ);
    out->time_valid=Cal_Valid(&out->now);
    out->stale=true;
}

static uint32_t day_key(const CalDate *d)
{ return (uint32_t)d->year*10000U+d->month*100U+d->day; }

/* A single sequential socket owns response[]. W6X's HTTP helper reports
 * completion before the body and does not decode chunked responses; use the
 * supported socket API plus our bounded, host-tested HTTP framing instead. */
static bool http_json(const char *host,const char *cert,const char *name,
                      const char *method,const char *uri,const char *token,
                      const char *payload,char **json)
{
    if (!host || !*host || !cert || !*cert || strchr(host,'\r') || strchr(host,'\n') ||
        (token && (strchr(token,'\r') || strchr(token,'\n')))) return false;
    uint8_t ip[4];
    if (W6X_Net_ResolveHostAddress(host,ip)!=W6X_STATUS_OK) return false;
    int32_t sock=W6X_Net_Socket(AF_INET,SOCK_STREAM,IPPROTO_TLS_1_2);
    if (sock<0) return false;
    bool ok=false,credential=false;
    /* Reserve the socket's own credential tag and release it on every exit. */
    uint32_t tag=(uint32_t)sock;
    int32_t timeout=5000,rx_size=4096;
    if (W6X_Net_TLS_Credential_AddByContent(tag,W6X_NET_TLS_CREDENTIAL_CA_CERTIFICATE,
                                           name,cert,strlen(cert))!=0) goto done;
    credential=true;
    /* ST v1.3 expects a COUNT here, not sizeof(tag): see w6x_net.c. */
    if (W6X_Net_Setsockopt(sock,SOL_TLS,TLS_SEC_TAG_LIST,&tag,1)!=0 ||
        W6X_Net_Setsockopt(sock,SOL_TLS,TLS_HOSTNAME,host,strlen(host))!=0 ||
        W6X_Net_Setsockopt(sock,SOL_TLS,TLS_ALPN_LIST,"http/1.1",1)!=0 ||
        W6X_Net_Setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout))!=0 ||
        W6X_Net_Setsockopt(sock,SOL_SOCKET,SO_RCVBUF,&rx_size,sizeof(rx_size))!=0) goto done;
    struct sockaddr_in address={0};
    address.sin_family=AF_INET; address.sin_port=PP_HTONS(443);
    memcpy(&address.sin_addr.s_addr,ip,sizeof(ip));
    if (W6X_Net_Connect(sock,(struct sockaddr *)&address,sizeof(address))!=0) goto done;
    char request[1024];
    int n=snprintf(request,sizeof(request),"%s %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: AuraAssistant/1.0\r\nAccept: application/json\r\nAccept-Encoding: identity\r\n%s%s%sContent-Type: application/json\r\nContent-Length: %u\r\nConnection: close\r\n\r\n%s",
        method,uri,host,token?"Authorization: Bearer ":"",token?token:"",token?"\r\n":"",
        (unsigned)(payload?strlen(payload):0),payload?payload:"");
    if (n<=0 || (size_t)n>=sizeof(request)) goto done;
    size_t sent=0;
    while (sent<(size_t)n) {
        ssize_t written=W6X_Net_Send(sock,request+sent,n-sent,0);
        if (written<=0) goto done;
        sent+=(size_t)written;
    }
    size_t used=0,body_size=0; char *body=NULL;
    TickType_t start=xTaskGetTickCount();
    while (used<RESPONSE_CAP && (TickType_t)(xTaskGetTickCount()-start)<pdMS_TO_TICKS(20000)) {
        size_t want=RESPONSE_CAP-used; if (want>1024) want=1024;
        ssize_t got=W6X_Net_Recv(sock,state.response+used,want,0);
        /* W6X returns zero on a receive timeout, not on a POSIX EOF.
         * Only explicit Content-Length/chunk terminators complete a response. */
        if (got<=0) goto done;
        used+=(size_t)got;
        int result=Cal_HttpBody(state.response,used,false,false,&body,&body_size);
        if (result<0) goto done;
        if (result==1) {
            if (Cal_HttpBody(state.response,used,false,true,&body,&body_size)!=1) goto done;
            if (memchr(body,0,body_size)) goto done;
            body[body_size]=0; *json=body; ok=true; break;
        }
    }
done:
    (void)W6X_Net_Close(sock);
    if (credential) (void)W6X_Net_TLS_Credential_Delete(tag,W6X_NET_TLS_CREDENTIAL_CA_CERTIFICATE);
    return ok;
}

static bool get_json(bool holidays,unsigned year,char **json)
{
    char uri[100];
    if (holidays) snprintf(uri,sizeof(uri),"/v1/holidays?country=AE&year=%u&type=public",year);
    else strcpy(uri,"/api/Time/current/zone?timeZone=Asia%2FDubai");
    return http_json(holidays?"worldtimeandweather.com":"timeapi.io",
        holidays?calendar_holidays_ca:calendar_time_ca,
        holidays?"cal_holidays_ca.pem":"cal_time_ca.pem","GET",uri,NULL,NULL,json);
}

static void web_run(void *arg)
{
    (void)arg;
    uint32_t retry=APP_WEB_POLL_SECONDS;
    bool synced=false,sntp=false;
    for (;;) {
        bool online; uint16_t wanted;
        taskENTER_CRITICAL(); online=state.online; wanted=state.requested_year; taskEXIT_CRITICAL();
        uint32_t wait=APP_WEB_POLL_SECONDS;
        if (online) {
            if (!sntp) sntp=W6X_Net_SNTP_SetConfiguration(1,0,(uint8_t *)"pool.ntp.org",NULL,NULL)==W6X_STATUS_OK;
            if (!wanted) {
                CalSnapshot clock; APP_CalendarRead(&clock);
                wanted=clock.time_valid?clock.now.year:CAL_FIRST_YEAR;
            }
            char uri[128],*json=NULL;
            bool ok=true; uint32_t id;
            // One pending completion per poll, retried idempotently on transport failure.
            bool completed;
            bool reminderChange=false;
            if (APP_RemindersNextChange(&id,&completed)) reminderChange=true;
            if (reminderChange || APP_TasksNextChange(&id,&completed)) {
                snprintf(uri,sizeof(uri),"/api/device/v1/%s/%lu/completion",reminderChange?"reminders":"tasks",(unsigned long)id);
                bool accepted=false;
                ok=http_json(APP_WEB_HOST,APP_WEB_ROOT_CA_PEM,"aura_web_ca.pem","PUT",uri,
                    APP_WEB_DEVICE_TOKEN,completed?"{\"completed\":true}":"{\"completed\":false}",&json)&&Web_ParseCompletion(json,id,&accepted);
                if (ok) {
                    if (reminderChange) (void)APP_RemindersAcknowledgeChange(id,completed,accepted);
                    else (void)APP_TasksAcknowledgeChange(id,completed,accepted);
                }
            }
            snprintf(uri,sizeof(uri),"/api/device/v1/state?calendar_year=%u",wanted);
            ok=ok && http_json(APP_WEB_HOST,APP_WEB_ROOT_CA_PEM,"aura_web_ca.pem","GET",uri,
                APP_WEB_DEVICE_TOKEN,NULL,&json)&&Web_ParseState(json,wanted,&state.web);
            if (ok) {
                // Repeated revisions are valid polls, but older snapshots are rejected.
                // Tasks store rejects an equal revision without resetting pending work.
                if (!synced || state.web.revision>=state.sync_day) {
                    (void)APP_TasksPublish(state.web.tasks,state.web.count,state.web.revision);
                    if (state.web.reminders_present)
                        (void)APP_RemindersPublish(state.web.reminders,state.web.reminder_count,state.web.revision);
                    taskENTER_CRITICAL();
                    state.shared.now=state.web.now; state.shared.time_valid=true;
                    state.shared.holidays=state.web.holidays; state.shared.holidays_valid=true;
                    state.shared.stale=false; state.shared.online=state.online;
                    state.shared_tick=state.clock_tick=xTaskGetTickCount();
                    state.sync_day=state.web.revision;
                    taskEXIT_CRITICAL();
                    synced=true;
                } else ok=false;
            }
            if (ok) retry=APP_WEB_POLL_SECONDS;
            else { wait=retry; retry=retry<30?retry*2:60; taskENTER_CRITICAL(); state.shared.stale=true; taskEXIT_CRITICAL(); }
        } else { taskENTER_CRITICAL(); state.shared.stale=true; taskEXIT_CRITICAL(); }
        osDelay(pdMS_TO_TICKS(wait*1000));
    }
}

static void advance_clock(CalSnapshot *s)
{
    uint32_t tick=xTaskGetTickCount();
    uint32_t seconds=(uint32_t)(tick-state.clock_tick)/configTICK_RATE_HZ;
    state.clock_tick+=seconds*configTICK_RATE_HZ;
    if (s->time_valid) { Cal_Advance(&s->now,seconds); s->time_valid=Cal_Valid(&s->now); }
}
static void run(void *arg)
{
    (void)arg;
    CalSnapshot snapshot=state.shared;
    uint32_t wait_seconds=0,retry=30;
    bool ncp_time_configured=false;
    for (;;) {
        advance_clock(&snapshot);
        bool online; uint16_t wanted;
        taskENTER_CRITICAL(); online=state.online; wanted=state.requested_year; taskEXIT_CRITICAL();
        uint32_t day=snapshot.time_valid?day_key(&snapshot.now):0;
        if (wait_seconds) --wait_seconds;
        if (online && !wait_seconds) {
            char *json;
            bool success=true;
            if (!snapshot.time_valid || state.sync_day!=day) {
                /* The NCP needs UTC for certificate validity checks. The
                 * HTTP replaces the boot fallback with synchronized time. */
                if (!ncp_time_configured)
                    ncp_time_configured=W6X_Net_SNTP_SetConfiguration(1,0,
                        (uint8_t *)"pool.ntp.org",NULL,NULL)==W6X_STATUS_OK;
                CalDate now;
                success=get_json(false,0,&json)&&Cal_ParseTime(json,&now);
                if (success) {
                    snapshot.now=now; snapshot.time_valid=true;
                    state.clock_tick=xTaskGetTickCount();
                    day=day_key(&now); state.sync_day=day;
                }
            }
            if (success && snapshot.time_valid) {
                if (!wanted) wanted=snapshot.now.year;
                /* Refresh current year daily even while viewing another year. */
                unsigned years[2]={snapshot.now.year,wanted};
                for (unsigned k=0;k<2 && success;k++) {
                    Cache *slot=NULL;
                    for (unsigned i=0;i<3;i++) if (state.cache[i].holidays.year==years[k]) slot=&state.cache[i];
                    if (slot && slot->day==day) continue;
                    if (!slot) {
                        for (unsigned i=0;i<3;i++) if (!state.cache[i].holidays.year) { slot=&state.cache[i]; break; }
                        if (!slot) for (unsigned i=0;i<3;i++) if (state.cache[i].holidays.year!=snapshot.now.year && state.cache[i].holidays.year!=wanted) { slot=&state.cache[i]; break; }
                    }
                    CalHolidays fresh;
                    success=slot && get_json(true,years[k],&json)&&Cal_ParseHolidays(json,years[k],&fresh);
                    if (success) { slot->holidays=fresh; slot->day=day; }
                }
            }
            if (success) retry=30;
            else { wait_seconds=retry; retry=retry<1800?retry*2:3600; }
        }
        advance_clock(&snapshot);
        day=snapshot.time_valid?day_key(&snapshot.now):0;
        if (!wanted) wanted=snapshot.now.year;
        snapshot.holidays_valid=false;
        snapshot.stale=state.sync_day!=day;
        for (unsigned i=0;i<3;i++) if (state.cache[i].holidays.year==wanted && wanted) {
            snapshot.holidays=state.cache[i].holidays;
            snapshot.holidays_valid=true;
            snapshot.stale|=state.cache[i].day!=day;
        }
        taskENTER_CRITICAL();
        snapshot.online=state.online;
        state.shared=snapshot;
        state.shared_tick=state.clock_tick;
        taskEXIT_CRITICAL();
        osDelay(pdMS_TO_TICKS(1000));
    }
}
bool APP_CalendarStart(void)
{
    if (worker) return true;
    memset(&state,0,sizeof(state));
    boot_snapshot(&state.shared);
    state.clock_tick=state.shared_tick=xTaskGetTickCount();
    const osThreadAttr_t attr={.name="Calendar",.stack_mem=calendar_stack,.stack_size=sizeof(calendar_stack),
        .cb_mem=&calendar_task,.cb_size=sizeof(calendar_task),.priority=osPriorityBelowNormal};
    worker=osThreadNew(APP_WEB_ENABLED?web_run:run,NULL,&attr);
    return worker!=NULL;
}
void APP_CalendarSetOnline(bool online)
{ if (worker) { taskENTER_CRITICAL(); state.online=online; taskEXIT_CRITICAL(); } }
void APP_CalendarRequestYear(uint16_t year)
{
    if (worker && year>=CAL_FIRST_YEAR && year<=CAL_LAST_YEAR) {
        taskENTER_CRITICAL(); state.requested_year=year; taskEXIT_CRITICAL();
    }
}
void APP_CalendarRead(CalSnapshot *out)
{
    if (!out) return;
    if (!worker) { boot_snapshot(out); return; }
    uint32_t sampled;
    taskENTER_CRITICAL();
    *out=state.shared;
    out->online=state.online;
    sampled=state.shared_tick;
    taskEXIT_CRITICAL();
    /* Keep HH:MM moving even during DNS/TLS/receive timeouts in the worker. */
    if (out->time_valid) {
        Cal_Advance(&out->now,(uint32_t)(xTaskGetTickCount()-sampled)/configTICK_RATE_HZ);
        out->time_valid=Cal_Valid(&out->now);
    }
}
