#include "app_tasks.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
/* CPU-owned data in SRAM4, never a DMA buffer. Explicit NOLOAD init. */
static TaskStore store __attribute__((section(".tasks"),aligned(8)));
static bool demo_mode;
/* Offline bring-up fixtures; replaced by the first valid web snapshot.
   Fixed dates remain honest when no synchronized clock is available. */
static const TaskRecord demo_tasks[] = {
    {1,{2026,10,6,9,0,0},"Prepare project report",TASK_PROJECT,false},
    {2,{2026,10,6,10,30,0},"Team meeting",TASK_WORK,false},
    {3,{2026,10,6,14,0,0},"Review PCB design",TASK_PRIORITY,false},
    {4,{2026,10,6,16,0,0},"Plan training session",TASK_PERSONAL,false},
    {5,{2026,10,7,9,0,0},"Send project update",TASK_WORK,false},
    {6,{2026,10,7,11,0,0},"Order PCB components",TASK_PROJECT,false},
    {7,{2026,10,8,14,0,0},"Review power budget",TASK_PRIORITY,false},
    {8,{2026,10,10,10,0,0},"Plan weekend",TASK_PERSONAL,false}
};
void APP_TasksInit(void)
{
    memset(&store,0,sizeof(store));
    demo_mode=Tasks_Publish(&store,demo_tasks,
                           sizeof(demo_tasks)/sizeof(demo_tasks[0]),0);
}
bool APP_TasksPublish(const TaskRecord *r,size_t n,uint32_t revision)
{
    taskENTER_CRITICAL();
    /* The demo revision must not gate a real snapshot, including revision 0. */
    bool loaded=store.loaded;
    if (demo_mode) store.loaded=false;
    bool ok=Tasks_Publish(&store,r,n,revision);
    if (ok) demo_mode=false;
    else store.loaded=loaded;
    taskEXIT_CRITICAL();
    return ok;
}
void APP_TasksReadPage(unsigned page,TaskPage *out)
{
    taskENTER_CRITICAL(); Tasks_ReadPage(&store,page,out); taskEXIT_CRITICAL();
}
bool APP_TasksComplete(uint32_t id)
{
    taskENTER_CRITICAL();
    bool ok=Tasks_Complete(&store,id);
    /* Demo completions stay local and must never be sent to the website. */
    if (ok && demo_mode) (void)Tasks_Acknowledge(&store,id,true);
    taskEXIT_CRITICAL();
    return ok;
}
bool APP_TasksNextCompletion(uint32_t *id)
{
    taskENTER_CRITICAL(); bool ok=Tasks_NextPending(&store,id); taskEXIT_CRITICAL(); return ok;
}
bool APP_TasksToggle(uint32_t id,bool *completed)
{
    taskENTER_CRITICAL();
    bool ok=Tasks_Toggle(&store,id,completed);
    if (ok && demo_mode) (void)Tasks_AcknowledgeChange(&store,id,*completed,true);
    taskEXIT_CRITICAL(); return ok;
}
bool APP_TasksNextChange(uint32_t *id,bool *completed)
{
    taskENTER_CRITICAL(); bool ok=Tasks_NextChange(&store,id,completed); taskEXIT_CRITICAL(); return ok;
}
bool APP_TasksAcknowledgeChange(uint32_t id,bool completed,bool accepted)
{
    taskENTER_CRITICAL(); bool ok=Tasks_AcknowledgeChange(&store,id,completed,accepted); taskEXIT_CRITICAL(); return ok;
}
bool APP_TasksAcknowledge(uint32_t id,bool accepted)
{
    taskENTER_CRITICAL(); bool ok=Tasks_Acknowledge(&store,id,accepted); taskEXIT_CRITICAL(); return ok;
}
