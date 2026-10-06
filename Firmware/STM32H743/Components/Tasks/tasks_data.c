#include "tasks_data.h"
#include <stdio.h>
#include <string.h>
static int date_compare(const CalDate *a, const CalDate *b)
{
    const unsigned av[]={a->year,a->month,a->day,a->hour,a->minute,a->second};
    const unsigned bv[]={b->year,b->month,b->day,b->hour,b->minute,b->second};
    for (unsigned i=0;i<6;++i) if (av[i]!=bv[i]) return av[i]<bv[i]?-1:1;
    return 0;
}
static unsigned day_number(const CalDate *d)
{
    unsigned days=0;
    for (unsigned y=CAL_FIRST_YEAR;y<d->year;++y) days+=Cal_Days(y,2)==29?366:365;
    for (unsigned m=1;m<d->month;++m) days+=Cal_Days(d->year,m);
    return days+d->day-1;
}
uint8_t Tasks_ProgressSteps(uint8_t completed,uint8_t total)
{
    if (!total) return 0;
    if (completed>=total) return 40;
    return (uint8_t)((completed*40U+total/2U)/total);
}
bool Tasks_Publish(TaskStore *s,const TaskRecord *r,size_t n,uint32_t revision)
{
    if (!s || n>TASKS_CAPACITY || (n && !r) || (s->loaded && revision<=s->revision)) return false;
    for (size_t i=0;i<n;++i) {
        if (!r[i].id || !Cal_Valid(&r[i].deadline) || (unsigned)r[i].category>TASK_PROJECT ||
            !r[i].title[0] || !memchr(r[i].title,0,TASKS_TITLE_SIZE)) return false;
        for (size_t j=0;j<i;++j) if (r[i].id==r[j].id) return false;
    }
    uint32_t pending_ids[TASKS_CAPACITY];
    bool pending_values[TASKS_CAPACITY];
    unsigned pending_count=0;
    for (unsigned i=0;i<s->count;++i)
        if (s->pending & (UINT64_C(1)<<i)) {
            pending_ids[pending_count]=s->records[i].id;
            pending_values[pending_count++]=(s->desired & (UINT64_C(1)<<i))!=0;
        }
    s->pending=s->desired=0;
    for (size_t i=0;i<n;++i) {
        s->records[i]=r[i]; s->order[i]=(uint8_t)i;
        for (unsigned j=0;j<pending_count;++j)
            if (pending_ids[j]==r[i].id && pending_values[j]!=r[i].completed) {
                s->pending|=UINT64_C(1)<<i;
                if (pending_values[j]) s->desired|=UINT64_C(1)<<i;
            }
    }
    /* Stable insertion sort by full deadline, then ID for reproducible ties. */
    for (unsigned i=1;i<n;++i) {
        uint8_t index=s->order[i]; unsigned j=i;
        while (j) {
            const TaskRecord *a=&s->records[s->order[j-1]],*b=&s->records[index];
            int c=date_compare(&a->deadline,&b->deadline);
            if (c<0 || (c==0 && a->id<=b->id)) break;
            s->order[j]=s->order[j-1]; --j;
        }
        s->order[j]=index;
    }
    s->count=(uint8_t)n; s->revision=revision; s->loaded=true; ++s->version;
    return true;
}
void Tasks_ReadPage(const TaskStore *s,unsigned page,TaskPage *out)
{
    if (!s || !out) return;
    memset(out,0,sizeof(*out));
    unsigned pages=(s->count+TASKS_PAGE_SIZE-1)/TASKS_PAGE_SIZE;
    if (!pages) page=0; else if (page>=pages) page=pages-1;
    out->page=(uint8_t)page; out->total=s->count; out->loaded=s->loaded; out->version=s->version;
    for (unsigned i=0;i<s->count;++i) {
        uint64_t bit=UINT64_C(1)<<i;
        bool completed=(s->pending&bit)?(s->desired&bit)!=0:s->records[i].completed;
        if (completed) ++out->completed_count;
    }
    for (unsigned i=page*TASKS_PAGE_SIZE;i<s->count && out->count<TASKS_PAGE_SIZE;++i) {
        unsigned index=s->order[i];
        TaskRecord *r=&out->rows[out->count++]; *r=s->records[index];
        if (s->pending & (UINT64_C(1)<<index)) r->completed=(s->desired & (UINT64_C(1)<<index))!=0;
    }
}
bool Tasks_Complete(TaskStore *s,uint32_t id)
{
    if (!s) return false;
    for (unsigned i=0;i<s->count;++i) if (s->records[i].id==id) {
        uint64_t bit=UINT64_C(1)<<i;
        bool completed=(s->pending&bit)?(s->desired&bit)!=0:s->records[i].completed;
        if (completed) return false;
        s->pending|=bit; s->desired|=bit; ++s->version; return true;
    }
    return false;
}
bool Tasks_Toggle(TaskStore *s,uint32_t id,bool *completed)
{
    if (!s || !completed) return false;
    for (unsigned i=0;i<s->count;++i) if (s->records[i].id==id) {
        uint64_t bit=UINT64_C(1)<<i;
        bool current=(s->pending&bit)?(s->desired&bit)!=0:s->records[i].completed;
        *completed=!current;
        s->pending|=bit;
        if (*completed) s->desired|=bit; else s->desired&=~bit;
        ++s->version; return true;
    }
    return false;
}
bool Tasks_NextChange(const TaskStore *s,uint32_t *id,bool *completed)
{
    if (!s || !id || !completed) return false;
    for (unsigned i=0;i<s->count;++i) if (s->pending&(UINT64_C(1)<<i)) {
        *id=s->records[i].id; *completed=(s->desired&(UINT64_C(1)<<i))!=0; return true;
    }
    return false;
}
bool Tasks_AcknowledgeChange(TaskStore *s,uint32_t id,bool completed,bool accepted)
{
    if (!s) return false;
    for (unsigned i=0;i<s->count;++i) if (s->records[i].id==id) {
        uint64_t bit=UINT64_C(1)<<i;
        if (!(s->pending&bit)) return false;
        /* A click while the PUT was in flight retains its newer desired state. */
        if (accepted) s->records[i].completed=completed;
        if (((s->desired&bit)!=0)==completed) { s->pending&=~bit; s->desired&=~bit; }
        ++s->version; return true;
    }
    return false;
}
bool Tasks_NextPending(const TaskStore *s,uint32_t *id)
{
    if (!s || !id) return false;
    for (unsigned i=0;i<s->count;++i) if (s->pending&(UINT64_C(1)<<i)) {
        *id=s->records[i].id; return true;
    }
    return false;
}
bool Tasks_Acknowledge(TaskStore *s,uint32_t id,bool accepted)
{
    if (!s) return false;
    for (unsigned i=0;i<s->count;++i) if (s->records[i].id==id && (s->pending&(UINT64_C(1)<<i))) {
        return Tasks_AcknowledgeChange(s,id,(s->desired&(UINT64_C(1)<<i))!=0,accepted);
    }
    return false;
}
bool Tasks_DeadlineText(const CalDate *d,const CalDate *now,bool clock_valid,char *out,size_t capacity)
{
    if (!out || !capacity) return false;
    out[0]=0;
    if (!d || !Cal_Valid(d)) return false;
    int delta=99;
    if (clock_valid && now && Cal_Valid(now)) delta=(int)day_number(d)-(int)day_number(now);
    int n;
    if (delta==0) n=snprintf(out,capacity,"%02u:%02u",d->hour,d->minute);
    else if (delta==-1 || delta==1)
        n=snprintf(out,capacity,"%s \xe2\x80\xa2 %02u:%02u",delta==-1?"Yesterday":"Tomorrow",d->hour,d->minute);
    else n=snprintf(out,capacity,"%02u/%02u \xe2\x80\xa2 %02u:%02u",d->day,d->month,d->hour,d->minute);
    return n>=0 && (size_t)n<capacity;
}
