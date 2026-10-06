#include "calendar_data.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "web_state.h"


unsigned Cal_Days(unsigned y, unsigned m)
{
    static const uint8_t days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (m < 1 || m > 12) return 0;
    return days[m-1] + (m == 2 && y%4 == 0 && (y%100 != 0 || y%400 == 0));
}
bool Cal_Valid(const CalDate *d)
{
    return d && d->year >= CAL_FIRST_YEAR && d->year <= CAL_LAST_YEAR &&
        d->day >= 1 && d->day <= Cal_Days(d->year,d->month) &&
        d->hour < 24 && d->minute < 60 && d->second < 60;
}
unsigned Cal_Weekday(unsigned y, unsigned m, unsigned d)
{
    static const unsigned t[] = {0,3,2,5,0,3,5,1,4,6,2,4};
    if (m < 1 || m > 12) return 0;
    y -= m < 3;
    return (y+y/4-y/100+y/400+t[m-1]+d+6)%7;
}
void Cal_Advance(CalDate *d, uint32_t seconds)
{
    if (!Cal_Valid(d)) return;
    uint32_t s = d->hour*3600U+d->minute*60U+d->second;
    uint32_t days = seconds/86400U;
    s += seconds%86400U;
    days += s/86400U; s %= 86400U;
    d->hour = s/3600U; d->minute = s/60U%60U; d->second = s%60U;
    while (days--) {
        if (++d->day > Cal_Days(d->year,d->month)) {
            d->day = 1;
            if (++d->month > 12) { d->month=1; ++d->year; }
        }
    }
}

/* Bounded-depth JSON walker. No heap, no substring matching inside strings.
 * Unknown fields are skipped after validating their complete JSON grammar. */
typedef struct { const char *begin, *end; } Span;
static void ws(const char **p) { while (**p && strchr(" \r\n\t",**p)) ++*p; }
static bool string(const char **p)
{
    if (*(*p)++ != '"') return false;
    while (**p && **p != '"') {
        unsigned char c=(unsigned char)*(*p)++;
        if (c<32) return false;
        if (c=='\\') {
            c=(unsigned char)**p;
            if (!c) return false;
            ++*p;
            if (c=='u') { for (int i=0;i<4;i++) { if (!isxdigit((unsigned char)**p)) return false; ++*p; } }
            else if (!strchr("\"\\/bfnrt",c)) return false;
        }
    }
    if (**p != '"') return false;
    ++*p; return true;
}
static bool value(const char **p, unsigned depth)
{
    ws(p);
    if (depth>12 || !**p) return false;
    if (**p=='"') return string(p);
    if (**p=='{' || **p=='[') {
        bool object=*(*p)++=='{'; char end=object?'}':']'; ws(p);
        if (**p==end) { ++*p; return true; }
        for (;;) {
            if (object) { if (**p!='"' || !string(p)) return false; ws(p); if (*(*p)++!=':') return false; }
            if (!value(p,depth+1)) return false;
            ws(p); if (**p==end) { ++*p; return true; }
            if (**p!=',') return false;
            ++*p; ws(p);
        }
    }
    const char *start=*p;
    if (**p=='-' || isdigit((unsigned char)**p)) {
        if (**p=='-') ++*p;
        if (**p=='0') ++*p;
        else { if (**p<'1'||**p>'9') return false; while (isdigit((unsigned char)**p)) ++*p; }
        if (**p=='.') { ++*p; if (!isdigit((unsigned char)**p)) return false; while (isdigit((unsigned char)**p)) ++*p; }
        if (**p=='e'||**p=='E') { ++*p; if (**p=='+'||**p=='-') ++*p; if (!isdigit((unsigned char)**p)) return false; while (isdigit((unsigned char)**p)) ++*p; }
        return true;
    }
    const char *words[]={"true","false","null"};
    for (unsigned i=0;i<3;i++) if (strncmp(start,words[i],strlen(words[i]))==0) { *p+=strlen(words[i]); return true; }
    return false;
}
static bool document(const char *json, Span *root)
{
    if (!json) return false;
    const char *p=json; ws(&p); root->begin=p;
    if (*p!='{' || !value(&p,0)) return false;
    root->end=p; ws(&p); return !*p;
}
static bool eq(Span s, const char *text)
{ return s.end-s.begin==(int)strlen(text)+2 && *s.begin=='"' && !memcmp(s.begin+1,text,strlen(text)); }
static bool field(Span obj, const char *key, Span *out)
{
    if (*obj.begin!='{') return false;
    const char *p=obj.begin+1; bool found=false; ws(&p);
    while (*p!='}') {
        Span k={p,p}; if (!string(&p)) return false; k.end=p;
        ws(&p); if (*p++!=':') return false; ws(&p);
        Span v={p,p}; if (!value(&p,0)) return false; v.end=p;
        if (eq(k,key)) { if (found) return false; *out=v; found=true; }
        ws(&p); if (*p=='}') break; if (*p++!=',') return false; ws(&p);
    }
    return found;
}
static bool number(Span obj,const char *key,unsigned *out)
{
    Span v; if (!field(obj,key,&v)||v.begin==v.end) return false;
    unsigned n=0;
    for (const char *p=v.begin;p<v.end;p++) { if (*p<'0'||*p>'9'||n>100000) return false; n=n*10+*p-'0'; }
    *out=n; return true;
}
bool Cal_ParseTime(const char *json, CalDate *out)
{
    Span r,z; unsigned y,m,d,h,mi,s;
    if (!out || !document(json,&r) || !field(r,"timeZone",&z) || !eq(z,"Asia/Dubai") ||
        !number(r,"year",&y)||!number(r,"month",&m)||!number(r,"day",&d)||
        !number(r,"hour",&h)||!number(r,"minute",&mi)||!number(r,"seconds",&s) ||
        y>65535 || m>255 || d>255 || h>255 || mi>255 || s>255) return false;
    CalDate temp={(uint16_t)y,(uint8_t)m,(uint8_t)d,(uint8_t)h,(uint8_t)mi,(uint8_t)s};
    if (!Cal_Valid(&temp)) return false;
    *out=temp; return true;
}
static bool date(Span s, CalDate *d)
{
    if (s.end-s.begin!=12 || s.begin[0]!='"' || s.begin[5]!='-' || s.begin[8]!='-') return false;
    unsigned n=0;
    for (int i=1;i<11;i++) { if (i==5||i==8) continue; if (s.begin[i]<'0'||s.begin[i]>'9') return false; n=n*10+s.begin[i]-'0'; }
    memset(d,0,sizeof(*d)); d->year=n/10000; d->month=n/100%100; d->day=n%100;
    return Cal_Valid(d);
}
bool Cal_ParseHolidays(const char *json, unsigned year, CalHolidays *out)
{
    Span r,v,a; unsigned y,count;
    if (!out || !document(json,&r)||!field(r,"country",&v)||!eq(v,"AE")||
        !number(r,"year",&y)||y!=year||!number(r,"count",&count)||count>64||
        !field(r,"holidays",&a)||*a.begin!='[') return false;
    CalHolidays result={0,{0},{0}}; result.year=y;
    /* A provider may return an empty list for years outside its dataset.
     * An unavailable year must not look like a confirmed year with no holidays. */
    Span available;
    if (!field(r,"years",&available) || *available.begin!='[') return false;
    const char *yp=available.begin+1;
    bool supported=false;
    ws(&yp);
    while (*yp!=']') {
        unsigned listed=0;
        if (!isdigit((unsigned char)*yp)) return false;
        while (isdigit((unsigned char)*yp)) { if (listed>10000) return false; listed=listed*10+*yp++-'0'; }
        supported|=listed==year;
        ws(&yp); if (*yp==']') break;
        if (*yp++!=',') return false;
        ws(&yp);
    }
    if (!supported) return false;
    const char *p=a.begin+1; ws(&p); unsigned seen=0;
    while (*p!=']') {
        Span obj={p,p},conf,kind,observed; CalDate d;
        if (!value(&p,0)) return false;
        obj.end=p;
        if (++seen>64 || !field(obj,"type",&kind)||!eq(kind,"public")||
            !field(obj,"date",&v)||!date(v,&d)||d.year!=y || !field(obj,"confidence",&conf)) return false;
        bool estimated=eq(conf,"estimated");
        if (!estimated&&!eq(conf,"verified")&&!eq(conf,"generated")) return false;
        /* Public observed dates are also non-working dates when provided. */
        result.days[d.month-1]|=1UL<<(d.day-1);
        if (estimated) result.estimated[d.month-1]|=1UL<<(d.day-1);
        if (field(obj,"observed",&observed) && *observed.begin=='"') {
            if (!date(observed,&d)) return false;
            if (d.year==y) { result.days[d.month-1]|=1UL<<(d.day-1); if (estimated) result.estimated[d.month-1]|=1UL<<(d.day-1); }
        }
        ws(&p); if (*p==']') break; if (*p++!=',') return false; ws(&p);
    }
    if (seen!=count || year<CAL_FIRST_YEAR||year>CAL_LAST_YEAR) return false;
    *out=result; return true;
}

static bool equal_ci(const char *s,size_t n,const char *t)
{
    if (n!=strlen(t)) return false;
    for (size_t i=0;i<n;i++) if (tolower((unsigned char)s[i])!=tolower((unsigned char)t[i])) return false;
    return true;
}
static const char *line_end(const char *s,const char *end)
{ for (;s+1<end;s++) if (s[0]=='\r'&&s[1]=='\n') return s; return NULL; }
int Cal_HttpBody(char *raw,size_t length,bool eof,bool decode,char **body,size_t *body_length)
{
    const char *end=raw+length,*e=line_end(raw,end);
    if (!e) return length>4096||eof?-1:0;
    if (e-raw<12 || (e-raw>12 && raw[12]!=' ') ||
        (memcmp(raw,"HTTP/1.1 200",12)!=0 && memcmp(raw,"HTTP/1.0 200",12)!=0)) return -1;
    const char *p=e+2; size_t content=0; bool has_length=false,chunked=false;
    for (;;) {
        e=line_end(p,end); if (!e) return length>4096||eof?-1:0;
        if (e-raw>4096) return -1;
        if (e==p) { p=e+2; break; }
        const char *colon=(const char *)memchr(p,':',(size_t)(e-p)); if (!colon) return -1;
        const char *v=colon+1; while (v<e&&(*v==' '||*v=='\t')) ++v;
        const char *ve=e; while (ve>v&&(ve[-1]==' '||ve[-1]=='\t')) --ve;
        if (equal_ci(p,colon-p,"Content-Length")) {
            if (has_length||v==ve) return -1;
            has_length=true;
            for (;v<ve;v++) {
                if (*v<'0'||*v>'9'||content>(32768U-(unsigned)(*v-'0'))/10U) return -1;
                content=content*10+*v-'0';
            }
            if (content>32768) return -1;
        } else if (equal_ci(p,colon-p,"Transfer-Encoding")) {
            if (chunked||!equal_ci(v,ve-v,"chunked")) return -1;
            chunked=true;
        } else if (equal_ci(p,colon-p,"Content-Encoding") && !equal_ci(v,ve-v,"identity")) return -1;
        p=e+2;
    }
    if (chunked&&has_length) return -1;
    char *start=raw+(p-raw); size_t total=0;
    if (!chunked) {
        size_t available=(size_t)(end-p);
        if (has_length && available<content) return eof?-1:0;
        if (!has_length&&!eof) return available>32768?-1:0;
        total=has_length?content:available;
        if (total>32768 || available!=total) return -1;
    } else {
        for (;;) {
            e=line_end(p,end); if (!e) return eof?-1:0;
            size_t n=0; const char *h=p;
            if (h==e) return -1;
            while (h<e&&*h!=';') {
                unsigned char c=(unsigned char)*h++; unsigned digit;
                if (c>='0'&&c<='9') digit=c-'0'; else if (c>='a'&&c<='f') digit=c-'a'+10;
                else if (c>='A'&&c<='F') digit=c-'A'+10; else return -1;
                if (n>(32768U-digit)/16U) return -1;
                n=n*16+digit;
            }
            if (h==p || *p==';' || n>32768-total) return -1;
            p=e+2;
            if (n==0) {
                do { e=line_end(p,end); if (!e) return eof?-1:0; bool last=e==p; p=e+2; if (last) break; } while (true);
                if (p!=end) return -1;
                break;
            }
            if ((size_t)(end-p)<n+2) return eof?-1:0;
            if (p[n]!='\r'||p[n+1]!='\n') return -1;
            if (decode) memmove(start+total,p,n);
            total+=n; p+=n+2;
        }
    }
    *body=start; *body_length=total;
    return 1;
}

/* Device web contract reuses the validated bounded JSON walker above. */
static bool web_u32(Span object,const char *key,uint32_t *out)
{
    Span s; if (!field(object,key,&s) || s.begin==s.end) return false;
    uint32_t n=0;
    if (s.end-s.begin>1 && *s.begin=='0') return false;
    for (const char *p=s.begin;p<s.end;++p) {
        if (*p<'0' || *p>'9' || n>(UINT32_MAX-(unsigned)(*p-'0'))/10) return false;
        n=n*10+(unsigned)(*p-'0');
    }
    *out=n; return true;
}
static bool web_bool(Span object,const char *key,bool *out)
{
    Span s; if (!field(object,key,&s)) return false;
    if (s.end-s.begin==4 && !memcmp(s.begin,"true",4)) { *out=true; return true; }
    if (s.end-s.begin==5 && !memcmp(s.begin,"false",5)) { *out=false; return true; }
    return false;
}
static bool web_text(Span object,const char *key,char *out,size_t capacity)
{
    Span s; if (!field(object,key,&s) || s.end-s.begin<2 || *s.begin!='"') return false;
    size_t n=0;
    for (const char *p=s.begin+1;p<s.end-1;++p) {
        unsigned char c=(unsigned char)*p;
        if (c=='\\') {
            ++p; if (p>=s.end-1 || !strchr("\"\\/",*p)) return false; c=(unsigned char)*p;
        }
        if (c<32 || c>126 || n+1>=capacity) return false;
        out[n++]=(char)c;
    }
    out[n]=0; return n>0;
}
static bool web_date(Span object,CalDate *out)
{
    uint32_t y,m,d,h,mi,s;
    if (!web_u32(object,"year",&y)||!web_u32(object,"month",&m)||!web_u32(object,"day",&d)||
        !web_u32(object,"hour",&h)||!web_u32(object,"minute",&mi)||!web_u32(object,"second",&s)||
        y>65535||m>255||d>255||h>255||mi>255||s>255) return false;
    CalDate date_value={(uint16_t)y,(uint8_t)m,(uint8_t)d,(uint8_t)h,(uint8_t)mi,(uint8_t)s};
    if (!Cal_Valid(&date_value)) return false;
    *out=date_value; return true;
}
static bool web_records(Span items,TaskRecord *records,uint8_t *count)
{
    if (*items.begin!='[') return false;
    const char *p=items.begin+1; ws(&p);
    while (*p!=']') {
        if (*count==TASKS_CAPACITY) return false;
        Span item={p,p},due,category; if(!value(&p,0)) return false; item.end=p;
        TaskRecord *task=&records[*count];
        if (!web_u32(item,"id",&task->id)||!task->id||!web_text(item,"title",task->title,sizeof(task->title))||
            !field(item,"deadline",&due)||!web_date(due,&task->deadline)||
            !field(item,"category",&category)||!web_bool(item,"completed",&task->completed)) return false;
        if(eq(category,"Personal")) task->category=TASK_PERSONAL;
        else if(eq(category,"Work")) task->category=TASK_WORK;
        else if(eq(category,"Priority")) task->category=TASK_PRIORITY;
        else if(eq(category,"Project")) task->category=TASK_PROJECT;
        else return false;
        for (unsigned i=0;i<*count;++i) if(records[i].id==task->id) return false;
        ++(*count);
        ws(&p); if(*p==']') break; if(*p++!=',') return false; ws(&p);
    }
    return true;
}
bool Web_ParseState(const char *json,unsigned requested_year,WebState *out)
{
    Span root,v,clock,calendar,tasks;
    uint32_t version,revision,year;
    if (!out || !document(json,&root)||!web_u32(root,"schemaVersion",&version)||version!=1||
        !web_u32(root,"revision",&revision)||!revision||!field(root,"timeZone",&v)||!eq(v,"Asia/Dubai")||
        !field(root,"clock",&clock)||!field(root,"calendar",&calendar)||!field(root,"tasks",&tasks)||
        !web_u32(calendar,"year",&year)||year!=requested_year||*tasks.begin!='[') return false;
    memset(out,0,sizeof(*out)); out->revision=revision;
    if (!web_date(clock,&out->now)) return false;
    out->holidays.year=(uint16_t)year;
    Span holidays;
    if (!field(calendar,"holidays",&holidays)||*holidays.begin!='[') return false;
    const char *p=holidays.begin+1; ws(&p);
    while (*p!=']') {
        Span item={p,p}; if(!value(&p,0)) return false; item.end=p;
        CalDate d; if(!date(item,&d)||d.year!=year) return false;
        out->holidays.days[d.month-1]|=UINT32_C(1)<<(d.day-1);
        ws(&p); if(*p==']') break; if(*p++!=',') return false; ws(&p);
    }
    if (!web_records(tasks,out->tasks,&out->count)) return false;
    Span reminders;
    out->reminders_present=field(root,"reminders",&reminders);
    if (out->reminders_present && !web_records(reminders,out->reminders,&out->reminder_count)) return false;
    return true;
}

bool Web_ParseCompletion(const char *json,uint32_t expected_id,bool *accepted)
{
    Span root; uint32_t id,version; bool result;
    if (!accepted||!document(json,&root)||!web_u32(root,"schemaVersion",&version)||version!=1||
        !web_u32(root,"id",&id)||id!=expected_id||!web_bool(root,"accepted",&result)) return false;
    *accepted=result; return true;
}
